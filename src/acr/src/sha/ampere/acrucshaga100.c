/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acrucshaga10x.c
 */

//
// Includes
//
#include "acr.h"
#include "dev_falcon_v4.h"
#include "dev_fbfalcon_pri.h"
#include "dev_master.h"
#include "dev_fb.h"
#include "dev_bus.h"
#include "dev_sec_csb.h"
#include "dev_gsp_csb.h"
#include "acr_objacrlib.h"

#ifdef ACR_BUILD_ONLY
#include "config/g_sha_private.h"
#else
#include "g_sha_private.h"
#endif

#include "acr_objsha.h"

/***********************************************************************************
  The SW program guilde is at
  https://confluence.lwpu.com/display/~eddichang/ACR+SHA+SW+Programming+Guide
***********************************************************************************/

static void       _shaSetInitVector_GA100(SHA_ID algoId)  ATTR_OVLY(OVL_NAME);
static LwBool     _shaCheckTaskConfig_GA100(SHA_CONTEXT *pShaContext, SHA_TASK_CONFIG *pTaskCfg)  ATTR_OVLY(OVL_NAME);
static ACR_STATUS _prepareHashFetchConfig_GA100(SHA_ID algoId, LwU32 *pLoopCount, LwBool *pHashEntry64, LwBool *pHalfWord)  ATTR_OVLY(OVL_NAME);

/*!
 * @brief Wait for SHA engine idle
 *
 *
 * @return ACR_OK if engine enter idle state successfuly.
 *         ACR_ERROR_SHA_ENG_ERROR if SHA halted status asserted
 *         ACR_ERROR_TIMEOUT if timeout detected
 */
ACR_STATUS
shaWaitEngineIdle_GA100
(
    LwU32 timeOut
)
{
    LwU32      reg;
    LwU32      lwrrTime;
    LwU32      startTime;

#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)

    startTime = ACR_REG_RD32(CSB, LW_CSEC_FALCON_PTIMER0);

    do
    {
        reg = ACR_REG_RD32(CSB, LW_CSEC_FALCON_SHA_STATUS);

        if(FLD_TEST_DRF(_CSEC, _FALCON_SHA, _STATUS_STATE, _HALTED, reg))
        {
            return ACR_ERROR_SHA_ENG_ERROR;
        }

        lwrrTime = ACR_REG_RD32(CSB, LW_CSEC_FALCON_PTIMER0);

        if ((lwrrTime - startTime) > timeOut)
        {
            return ACR_ERROR_SHA_WAIT_IDLE_TIMEOUT;
        }
    } while(FLD_TEST_DRF(_CSEC, _FALCON_SHA, _STATUS_STATE, _BUSY, reg));

    return ACR_OK;

#elif defined(ASB)

    startTime = ACR_REG_RD32(CSB, LW_CGSP_FALCON_PTIMER0);

    do
    {
        reg = ACR_REG_RD32(CSB, LW_CGSP_FALCON_SHA_STATUS);

        if(FLD_TEST_DRF(_CGSP, _FALCON_SHA, _STATUS_STATE, _HALTED, reg))
        {
            return ACR_ERROR_SHA_ENG_ERROR;
        }

        lwrrTime = ACR_REG_RD32(CSB, LW_CGSP_FALCON_PTIMER0);

        if ((lwrrTime - startTime) > timeOut)
        {
            return ACR_ERROR_SHA_WAIT_IDLE_TIMEOUT;
        }
    } while(FLD_TEST_DRF(_CGSP, _FALCON_SHA, _STATUS_STATE, _BUSY, reg));

    return ACR_OK;

#else
    ct_assert(0);
#endif

}


/*!
 * @brief Execute SHA engine soft reset and wait for engine idle
 *
 * @return ACR_OK if reset and enter idle state successfuly.
           ACR_ERROR_TIMEOUT if timeout detected
 *
 */
ACR_STATUS
shaEngineSoftReset_GA100
(
    LwU32 timeOut
)
{
    LwU32      reg;
    LwU32      lwrrTime;
    LwU32      startTime;

#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    // Check if SHA mutex is released to avoid break other SHA engine client's task
    reg = ACR_REG_RD32(CSB, LW_CSEC_FALCON_SHA_MUTEX_STATUS);

    if (!FLD_TEST_DRF(_CSEC_FALCON, _SHA_MUTEX_STATUS, _LOCKED, _INIT, reg))
    {
        return ACR_ERROR_SHA_MUTEX_ACQUIRED_ALREADY;
    }

    reg = ACR_REG_RD32(CSB, LW_CSEC_FALCON_SHA_OPERATION);
    reg = FLD_SET_DRF(_CSEC_FALCON, _SHA_OPERATION, _SOFTRESET, _ENABLE, reg);
    ACR_REG_WR32(CSB, LW_CSEC_FALCON_SHA_OPERATION, reg);

    startTime = ACR_REG_RD32(CSB, LW_CSEC_FALCON_PTIMER0);
    // wait for soft reset clear
    do
    {
        reg = ACR_REG_RD32(CSB, LW_CSEC_FALCON_SHA_OPERATION);

        lwrrTime = ACR_REG_RD32(CSB, LW_CSEC_FALCON_PTIMER0);

        if ((lwrrTime - startTime) > timeOut)
        {
            return ACR_ERROR_SHA_SW_RESET_TIMEOUT;
        }

    } while(FLD_TEST_DRF(_CSEC_FALCON, _SHA_OPERATION, _SOFTRESET, _ENABLE, reg));

    return shaWaitEngineIdle_HAL(pSha, SHA_ENGINE_IDLE_TIMEOUT);

#elif defined(ASB)

    // Check if SHA mutex is released to avoid break other SHA engine client's task
    reg = ACR_REG_RD32(CSB, LW_CGSP_FALCON_SHA_MUTEX_STATUS);

    if (!FLD_TEST_DRF(_CGSP_FALCON, _SHA_MUTEX_STATUS, _LOCKED, _INIT, reg))
    {
        return ACR_ERROR_SHA_MUTEX_ACQUIRED_ALREADY;
    }

    reg = ACR_REG_RD32(CSB, LW_CGSP_FALCON_SHA_OPERATION);
    reg = FLD_SET_DRF(_CGSP_FALCON, _SHA_OPERATION, _SOFTRESET, _ENABLE, reg);
    ACR_REG_WR32(CSB, LW_CGSP_FALCON_SHA_OPERATION, reg);

    startTime = ACR_REG_RD32(CSB, LW_CGSP_FALCON_PTIMER0);
    // wait for soft reset clear
    do
    {
        reg = ACR_REG_RD32(CSB, LW_CGSP_FALCON_SHA_OPERATION);

        lwrrTime = ACR_REG_RD32(CSB, LW_CGSP_FALCON_PTIMER0);

        if ((lwrrTime - startTime) > timeOut)
        {
            return ACR_ERROR_SHA_SW_RESET_TIMEOUT;
        }

    } while(FLD_TEST_DRF(_CGSP_FALCON, _SHA_OPERATION, _SOFTRESET, _ENABLE, reg));

    return shaWaitEngineIdle_HAL(pSha, SHA_ENGINE_IDLE_TIMEOUT);

#else
    ct_assert(0);
#endif

}

/*!
 * @brief Execute SHA engine halt
 *
 * @return ACR_OK if halt engine successfully
 *         ACR_ERROR_SHA_ENG_ERROR if failed.
 *
 */
ACR_STATUS
shaEngineHalt_GA100
(
    void
)
{
    LwU32      reg = 0;

#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)

    reg = ACR_REG_RD32(CSB, LW_CSEC_FALCON_SHA_OPERATION);
    reg = FLD_SET_DRF(_CSEC_FALCON, _SHA_OPERATION, _HALT, _ENABLE, reg);
    ACR_REG_WR32(CSB, LW_CSEC_FALCON_SHA_OPERATION, reg);

    reg = ACR_REG_RD32(CSB, LW_CSEC_FALCON_SHA_STATUS);

    if (FLD_TEST_DRF(_CSEC, _FALCON_SHA, _STATUS_STATE, _HALTED, reg))
    {
        return ACR_OK;
    }

    return ACR_ERROR_SHA_ENG_ERROR;

#elif defined(ASB)

    reg = ACR_REG_RD32(CSB, LW_CGSP_FALCON_SHA_OPERATION);
    reg = FLD_SET_DRF(_CGSP_FALCON, _SHA_OPERATION, _HALT, _ENABLE, reg);
    ACR_REG_WR32(CSB, LW_CGSP_FALCON_SHA_OPERATION, reg);

    reg = ACR_REG_RD32(CSB, LW_CGSP_FALCON_SHA_STATUS);

    if (FLD_TEST_DRF(_CGSP, _FALCON_SHA, _STATUS_STATE, _HALTED, reg))
    {
        return ACR_OK;
    }

    return ACR_ERROR_SHA_ENG_ERROR;

#else
    ct_assert(0);
#endif

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
ACR_STATUS
shaAcquireMutex_GA100
(
    LwU8 token
)
{
    LwU32 reg = 0;
    LwU8  val;

#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)

    reg = FLD_SET_DRF_NUM(_CSEC, _FALCON_SHA_MUTEX, _VAL, token, reg);
    ACR_REG_WR32(CSB, LW_CSEC_FALCON_SHA_MUTEX , reg);
    reg = ACR_REG_RD32(CSB, LW_CSEC_FALCON_SHA_MUTEX_STATUS);
    val = DRF_VAL(_CSEC, _FALCON_SHA_MUTEX_STATUS, _LOCKED, reg);

    return (val == token ? ACR_OK : ACR_ERROR_SHA_MUTEX_ACQUIRE_FAILED);

#elif defined(ASB)

    reg = FLD_SET_DRF_NUM(_CGSP, _FALCON_SHA_MUTEX, _VAL, token, reg);
    ACR_REG_WR32(CSB, LW_CGSP_FALCON_SHA_MUTEX , reg);
    reg = ACR_REG_RD32(CSB, LW_CGSP_FALCON_SHA_MUTEX_STATUS);
    val = DRF_VAL(_CGSP, _FALCON_SHA_MUTEX_STATUS, _LOCKED, reg);

    return (val == token ? ACR_OK : ACR_ERROR_SHA_MUTEX_ACQUIRE_FAILED);

#else
    ct_assert(0);
#endif

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
ACR_STATUS
shaReleaseMutex_GA100
(
    LwU8 token
)
{
    LwU32 reg = 0;
    LwU8  val;

#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)

    reg = FLD_SET_DRF_NUM(_CSEC, _FALCON_SHA_MUTEX_RELEASE, _VAL, token, reg);
    ACR_REG_WR32(CSB, LW_CSEC_FALCON_SHA_MUTEX_RELEASE, reg);
    reg = ACR_REG_RD32(CSB, LW_CSEC_FALCON_SHA_MUTEX_STATUS);
    val = DRF_VAL(_CSEC, _FALCON_SHA_MUTEX_STATUS, _LOCKED, reg);

    return  (val == LW_CSEC_FALCON_SHA_MUTEX_STATUS_LOCKED_INIT ? ACR_OK : ACR_ERROR_SHA_MUTEX_RELEASE_FAILED);

#elif defined(ASB)

    reg = FLD_SET_DRF_NUM(_CGSP, _FALCON_SHA_MUTEX_RELEASE, _VAL, token, reg);
    ACR_REG_WR32(CSB, LW_CGSP_FALCON_SHA_MUTEX_RELEASE, reg);
    reg = ACR_REG_RD32(CSB, LW_CGSP_FALCON_SHA_MUTEX_STATUS);
    val = DRF_VAL(_CGSP, _FALCON_SHA_MUTEX_STATUS, _LOCKED, reg);

    return  (val == LW_CGSP_FALCON_SHA_MUTEX_STATUS_LOCKED_INIT ? ACR_OK : ACR_ERROR_SHA_MUTEX_RELEASE_FAILED);

#else
    ct_assert(0);
#endif

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
ACR_STATUS
shaGetConfigEncodeMode_GA100
(
    SHA_ID algoId,
    LwU8  *pMode
)
{
    if (algoId >= SHA_ID_LAST)
    {
        return ACR_ERROR_SHA_ILWALID_CONFIG;
    }

#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)

    if (algoId == SHA_ID_SHA_1)
    {
        *pMode = LW_CSEC_FALCON_SHA_CONFIG_ENC_MODE_SHA1;
    }
    else if (algoId == SHA_ID_SHA_224)
    {
        *pMode = LW_CSEC_FALCON_SHA_CONFIG_ENC_MODE_SHA224;
    }
    else if (algoId == SHA_ID_SHA_256)
    {
        *pMode = LW_CSEC_FALCON_SHA_CONFIG_ENC_MODE_SHA256;
    }
    else if (algoId == SHA_ID_SHA_384)
    {
        *pMode = LW_CSEC_FALCON_SHA_CONFIG_ENC_MODE_SHA384;
    }
    else
    {
        *pMode = LW_CSEC_FALCON_SHA_CONFIG_ENC_MODE_SHA512;
    }

    return ACR_OK;

#elif defined(ASB)

    if (algoId == SHA_ID_SHA_1)
    {
        *pMode = LW_CGSP_FALCON_SHA_CONFIG_ENC_MODE_SHA1;
    }
    else if (algoId == SHA_ID_SHA_224)
    {
        *pMode = LW_CGSP_FALCON_SHA_CONFIG_ENC_MODE_SHA224;
    }
    else if (algoId == SHA_ID_SHA_256)
    {
        *pMode = LW_CGSP_FALCON_SHA_CONFIG_ENC_MODE_SHA256;
    }
    else if (algoId == SHA_ID_SHA_384)
    {
        *pMode = LW_CGSP_FALCON_SHA_CONFIG_ENC_MODE_SHA384;
    }
    else
    {
        *pMode = LW_CGSP_FALCON_SHA_CONFIG_ENC_MODE_SHA512;
    }

    return ACR_OK;

#else
    ct_assert(0);
#endif

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
ACR_STATUS
shaGetHashSizeByte_GA100
(
    SHA_ID  algoId,
    LwU32  *pSize
)
{
    if (pSize == NULL)
    {
        return ACR_ERROR_SHA_ILWALID_CONFIG;
    }

    if (algoId == SHA_ID_SHA_1)
    {
        *pSize = SHA_1_HASH_SIZE_BYTE;
    }
    else if (algoId == SHA_ID_SHA_224)
    {
        *pSize = SHA_224_HASH_SIZE_BYTE;
    }
    else if (algoId == SHA_ID_SHA_256)
    {
        *pSize = SHA_256_HASH_SIZE_BYTE;
    }
    else if (algoId == SHA_ID_SHA_384)
    {
        *pSize = SHA_384_HASH_SIZE_BYTE;
    }
    else if (algoId == SHA_ID_SHA_512)
    {
        *pSize = SHA_512_HASH_SIZE_BYTE;
    }
    else if (algoId == SHA_ID_SHA_512_224)
    {
        *pSize = SHA_512_224_HASH_SIZE_BYTE;

    }
    else if (algoId == SHA_ID_SHA_512_256)
    {
        *pSize = SHA_512_256_HASH_SIZE_BYTE;
    }
    else
    {
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
ACR_STATUS
shaOperationInit_GA100
(
    SHA_CONTEXT      *pShaContext
)
{
    LwU32 reg;
    LwU32 msgSize, i;
    LwU8  encMode;
    ACR_STATUS status;

#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)

    if (pShaContext == NULL)
    {
        return  ACR_ERROR_ILWALID_ARGUMENT;
    }

    //
    // Issue HALT when SHA state is IDLE or HALTED will be dropped silently.
    // So we only halt SHA engine when it's at busy state.
    //
    reg = ACR_REG_RD32(CSB, LW_CSEC_FALCON_SHA_STATUS);

    if (FLD_TEST_DRF(_CSEC, _FALCON_SHA, _STATUS_STATE, _BUSY, reg))
    {
        // Halt SHA engine
        shaEngineHalt_HAL(pSha);

        return ACR_ERROR_SHA_HW_UNSTABLE;
    }

    msgSize = pShaContext->msgSize;

    if (msgSize > SHA_MSG_BYTES_MAX)
    {
        return  ACR_ERROR_ILWALID_ARGUMENT;
    }

    status = shaGetConfigEncodeMode_GA100(pShaContext->algoId, &encMode);

    if (status != ACR_OK)
    {
        return  status;
    }

    reg = 0;
    reg = FLD_SET_DRF_NUM(_CSEC, _FALCON_SHA_CONFIG, _ENC_MODE, encMode, reg);
    reg = FLD_SET_DRF(_CSEC, _FALCON_SHA_CONFIG, _ENC_ALG, _SHA, reg);
    reg = FLD_SET_DRF(_CSEC, _FALCON_SHA_CONFIG, _DST, _HASH_REG, reg);
    ACR_REG_WR32(CSB, LW_CSEC_FALCON_SHA_CONFIG, reg);

    //
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // SW reset should clear LW_CSEC_FALCON_SHA_MSG_LENGTH(i) and LW_CSEC_FALCON_SHA_MSG_LEFT(i)
    // But we observer LW_CSEC_FALCON_SHA_MSG_LEFT(i) still has garbage values.
    // So we still clear these all registers to make sure operation able to work correctly.
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //
    for (i = 0; i < LW_CSEC_FALCON_SHA_MSG_LENGTH__SIZE_1; i++)
    {
        ACR_REG_WR32(CSB, LW_CSEC_FALCON_SHA_MSG_LENGTH(i), 0);
    }

    for (i = 0; i < LW_CSEC_FALCON_SHA_MSG_LEFT__SIZE_1; i++)
    {
        ACR_REG_WR32(CSB, LW_CSEC_FALCON_SHA_MSG_LEFT(i), 0);
    }

    //
    // In NIST spec, the MIN unit for SHA message is bit, but in real case, we just use byte unit.
    // Although HW provide 4 registers for messgae length nad left message length separately,
    // but lwrrently SW just usea one 32-bit register for SHA length(0x1FFFFFFF bytes)
    // In real case usage, openSSL supports byte unit as well.
    // And it should be enough for current requirement.
    //

    // support  SHA engine needs bit length
    ACR_REG_WR32(CSB, LW_CSEC_FALCON_SHA_MSG_LENGTH(0), BYTE_TO_BIT_LENGTH(msgSize));

    return ACR_OK;

#elif defined(ASB)

    if (pShaContext == NULL)
    {
        return  ACR_ERROR_ILWALID_ARGUMENT;
    }

    //
    // Issue HALT when SHA state is IDLE or HALTED will be dropped silently.
    // So we only halt SHA engine when it's at busy state.
    //
    reg = ACR_REG_RD32(CSB, LW_CGSP_FALCON_SHA_STATUS);

    if (FLD_TEST_DRF(_CGSP, _FALCON_SHA, _STATUS_STATE, _BUSY, reg))
    {
        // Halt SHA engine  
        shaEngineHalt_HAL(pSha);

        return ACR_ERROR_SHA_HW_UNSTABLE;
    }

    msgSize = pShaContext->msgSize;

    if (msgSize > SHA_MSG_BYTES_MAX)
    {
        return  ACR_ERROR_ILWALID_ARGUMENT;
    }

    status = shaGetConfigEncMode_GA100(pShaContext->algoId, &encMode);

    if (status != ACR_OK)
    {
        return  status;
    }

    reg = 0;
    reg = FLD_SET_DRF_NUM(_CGSP, _FALCON_SHA_CONFIG, _ENC_MODE, encMode, reg);
    reg = FLD_SET_DRF(_CGSP, _FALCON_SHA_CONFIG, _ENC_ALG, _SHA, reg);
    reg = FLD_SET_DRF(_CGSP, _FALCON_SHA_CONFIG, _DST, _HASH_REG, reg);
    ACR_REG_WR32(CSB, LW_CGSP_FALCON_SHA_CONFIG, reg);

    //
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // SW reset should clear LW_CSEC_FALCON_SHA_MSG_LENGTH(i) and LW_CSEC_FALCON_SHA_MSG_LEFT(i)
    // But we observer LW_CSEC_FALCON_SHA_MSG_LEFT(i) still has garbage values.
    // So we still clear these all registers to make sure operation able to work correctly.
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //
    for (i = 0; i < LW_CGSP_FALCON_SHA_MSG_LENGTH__SIZE_1; i++)
    {
        ACR_REG_WR32(CSB, LW_CGSP_FALCON_SHA_MSG_LENGTH(i), 0);
    }

    for (i = 0; i < LW_CSEC_FALCON_SHA_MSG_LEFT__SIZE_1; i++)
    {
        ACR_REG_WR32(CSB, LW_CGSP_FALCON_SHA_MSG_LEFT(i), 0);
    }

    //
    // In NIST spec, the MIN unit for SHA message is bit, but in real case, we just use byte unit.
    // Although HW provide 4 registers for messgae length nad left message length separately,
    // but lwrrently SW just usea one 32-bit register for SHA length(0x1FFFFFFF bytes)
    // In real case usage, openSSL supports byte unit as well.
    // And it should be enough for current requirement.
    //

    // support  SHA engine needs bit length
    ACR_REG_WR32(CSB, LW_CGSP_FALCON_SHA_MSG_LENGTH(0), BYTE_TO_BIT_LENGTH(msgSize));

    return ACR_OK;

#else
    ct_assert(0);
#endif

}

/*!
 * @brief To set initial vector for SHA-512-224 and SHA-512-256.
 *
 * @param[in] algoId The SHA algorithm id.
 *
 */
static void
_shaSetInitVector_GA100
(
    SHA_ID algoId
)
{

#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)

    if (algoId == SHA_ID_SHA_512_224)
    {
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(0), SHA_512_224_IV_0);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(1), SHA_512_224_IV_1);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(2), SHA_512_224_IV_2);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(3), SHA_512_224_IV_3);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(4), SHA_512_224_IV_4);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(5), SHA_512_224_IV_5);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(6), SHA_512_224_IV_6);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(7), SHA_512_224_IV_7);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(8), SHA_512_224_IV_8);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(9), SHA_512_224_IV_9);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(10), SHA_512_224_IV_10);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(11), SHA_512_224_IV_11);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(12), SHA_512_224_IV_12);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(13), SHA_512_224_IV_13);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(14), SHA_512_224_IV_14);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(15), SHA_512_224_IV_15);
    }
    else if (algoId == SHA_ID_SHA_512_256)
    {
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(0), SHA_512_256_IV_0);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(1), SHA_512_256_IV_1);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(2), SHA_512_256_IV_2);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(3), SHA_512_256_IV_3);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(4), SHA_512_256_IV_4);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(5), SHA_512_256_IV_5);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(6), SHA_512_256_IV_6);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(7), SHA_512_256_IV_7);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(8), SHA_512_256_IV_8);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(9), SHA_512_256_IV_9);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(10), SHA_512_256_IV_10);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(11), SHA_512_256_IV_11);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(12), SHA_512_256_IV_12);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(13), SHA_512_256_IV_13);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(14), SHA_512_256_IV_14);
        ACR_REG_WR32(CSB,  LW_CSEC_FALCON_SHA_HASH_RESULT(15), SHA_512_256_IV_15);
    }

#elif defined(ASB)

    if (algoId == SHA_ID_SHA_512_224)
    {
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(0), SHA_512_224_IV_0);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(1), SHA_512_224_IV_1);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(2), SHA_512_224_IV_2);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(3), SHA_512_224_IV_3);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(4), SHA_512_224_IV_4);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(5), SHA_512_224_IV_5);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(6), SHA_512_224_IV_6);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(7), SHA_512_224_IV_7);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(8), SHA_512_224_IV_8);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(9), SHA_512_224_IV_9);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(10), SHA_512_224_IV_10);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(11), SHA_512_224_IV_11);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(12), SHA_512_224_IV_12);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(13), SHA_512_224_IV_13);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(14), SHA_512_224_IV_14);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(15), SHA_512_224_IV_15);
    }
    else if (algoId == SHA_ID_SHA_512_256)
    {
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(0), SHA_512_256_IV_0);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(1), SHA_512_256_IV_1);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(2), SHA_512_256_IV_2);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(3), SHA_512_256_IV_3);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(4), SHA_512_256_IV_4);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(5), SHA_512_256_IV_5);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(6), SHA_512_256_IV_6);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(7), SHA_512_256_IV_7);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(8), SHA_512_256_IV_8);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(9), SHA_512_256_IV_9);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(10), SHA_512_256_IV_10);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(11), SHA_512_256_IV_11);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(12), SHA_512_256_IV_12);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(13), SHA_512_256_IV_13);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(14), SHA_512_256_IV_14);
        ACR_REG_WR32(CSB,  LW_CGSP_FALCON_SHA_HASH_RESULT(15), SHA_512_256_IV_15);
    }

#else
    ct_assert(0);
#endif

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
static LwBool
_shaCheckTaskConfig_GA100
(
    SHA_CONTEXT *pShaContext,
    SHA_TASK_CONFIG *pTaskCfg
)
{
    LwBool bValid = LW_TRUE;

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
        case SHA_ID_SHA_1:
        case SHA_ID_SHA_224:
        case SHA_ID_SHA_256:
            {
               bValid = LW_IS_ALIGNED(pTaskCfg->size, SHA_256_BLOCK_SIZE_BYTE);
            }
        break;

        case SHA_ID_SHA_384:
        case SHA_ID_SHA_512:
        case SHA_ID_SHA_512_224:
        case SHA_ID_SHA_512_256:
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
ACR_STATUS
shaInsertTask_GA100
(
    SHA_CONTEXT       *pShaContext,
    SHA_TASK_CONFIG   *pTaskCfg
)
{
    LwU32        reg, tmp;
    SHA_ID       algoId;
    ACR_STATUS   status = ACR_OK;
    LwU64        msgAddr;
    LwU32        leftMsgLength;

    if (pShaContext == NULL ||
        pTaskCfg == NULL)
    {
        return  ACR_ERROR_ILWALID_ARGUMENT;
    }

    algoId = pShaContext->algoId;
    leftMsgLength = pShaContext->msgSize;

#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)

    status = shaWaitEngineIdle_HAL(pSha, SHA_ENGINE_IDLE_TIMEOUT);

    if (status != ACR_OK)
    {
        return status;
    }

    if (pTaskCfg->size > leftMsgLength)
    {
        return ACR_ERROR_SHA_ILWALID_CONFIG;
    }

    if (!_shaCheckTaskConfig_GA100(pShaContext, pTaskCfg))
    {
        return ACR_ERROR_SHA_ILWALID_CONFIG;
    }

    // For each task, we need to set length for left message, bit unit.
    ACR_REG_WR32(CSB, LW_CSEC_FALCON_SHA_MSG_LEFT(0), BYTE_TO_BIT_LENGTH(leftMsgLength));

    // set source address
    msgAddr = pTaskCfg->addr;
    ACR_REG_WR32(CSB, LW_CSEC_FALCON_SHA_IN_ADDR , (LwU32)LwU64_LO32(msgAddr));

    reg = 0;
    reg = DRF_NUM(_CSEC, _FALCON_SHA_IN_ADDR_HI, _MSB, (LwU32)LwU64_HI32(msgAddr)) |
          DRF_NUM(_CSEC, _FALCON_SHA_IN_ADDR_HI, _SZ, pTaskCfg->size);
    ACR_REG_WR32(CSB, LW_CSEC_FALCON_SHA_IN_ADDR_HI, reg);


    // Set source config
    tmp = DRF_SIZE(LW_CSEC_FALCON_SHA_IN_ADDR_HI_MSB);
    tmp = ((LwU32)LwU64_HI32(msgAddr)) >> tmp;
    reg = DRF_NUM(_CSEC, _FALCON_SHA_SRC_CONFIG, _FB_BASE, tmp) |
          DRF_NUM(_CSEC, _FALCON_SHA_SRC_CONFIG, _CTXDMA, pTaskCfg->dmaIdx) |
          DRF_NUM(_CSEC, _FALCON_SHA_SRC_CONFIG, _SRC, pTaskCfg->srcType);
    ACR_REG_WR32(CSB, LW_CSEC_FALCON_SHA_SRC_CONFIG , reg);

    // Set task config
    reg = 0;
    if (pTaskCfg->defaultHashIv)
    {
        // sha512-224 ahs512-256, SW needs to set initial vector.
        if (algoId == SHA_ID_SHA_512_224 ||
            algoId == SHA_ID_SHA_512_256)
        {
            _shaSetInitVector_GA100(algoId);
            reg = FLD_SET_DRF(_CSEC_FALCON, _SHA_TASK_CONFIG, _HW_INIT_HASH,
                              _DISABLE, reg);
        }
        else
        {
            reg = FLD_SET_DRF(_CSEC_FALCON, _SHA_TASK_CONFIG, _HW_INIT_HASH,
                              _ENABLE, reg);
        }
    }
    else
    {
        reg = FLD_SET_DRF(_CSEC_FALCON, _SHA_TASK_CONFIG, _HW_INIT_HASH,
                          _DISABLE, reg);
    }
    ACR_REG_WR32(CSB, LW_CSEC_FALCON_SHA_TASK_CONFIG, reg);

    // Trigger OP start
    reg = 0;
    reg = FLD_SET_DRF(_CSEC_FALCON, _SHA_OPERATION, _LAST_BUF, _TRUE, reg);
    reg = FLD_SET_DRF(_CSEC_FALCON, _SHA_OPERATION, _OP, _START, reg);
    ACR_REG_WR32(CSB, LW_CSEC_FALCON_SHA_OPERATION, reg);

    status = shaWaitEngineIdle_HAL(pSha, SHA_ENGINE_IDLE_TIMEOUT);

    if (status != ACR_OK)
    {
        return status;
    }

    // Check if any error interrupt asserted
    reg = ACR_REG_RD32(CSB, LW_CSEC_FALCON_SHA_ERR_STATUS);

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

#elif defined(ASB)

    status = shaWaitEngineIdle_HAL(pSha, SHA_ENGINE_IDLE_TIMEOUT);

    if (status != ACR_OK)
    {
        return status;
    }

    if (pTaskCfg->size > leftMsgLength)
    {
        return ACR_ERROR_SHA_ILWALID_CONFIG;
    }

    if (!_shaCheckTaskConfig_GA100(pShaContext, pTaskCfg))
    {
        return ACR_ERROR_SHA_ILWALID_CONFIG;
    }

    // For each task, we need to set length for left message, bit unit.
    ACR_REG_WR32(CSB, LW_CGSP_FALCON_SHA_MSG_LEFT(0), BYTE_TO_BIT_LENGTH(leftMsgLength));

    // set source address
    msgAddr = pTaskCfg->addr;
    ACR_REG_WR32(CSB, LW_CGSP_FALCON_SHA_IN_ADDR , (LwU32)LwU64_LO32(msgAddr));

    reg = 0;
    reg = DRF_NUM(_CGSP, _FALCON_SHA_IN_ADDR_HI, _MSB, (LwU32)LwU64_HI32(msgAddr)) |
          DRF_NUM(_CGSP, _FALCON_SHA_IN_ADDR_HI, _SZ, pTaskCfg->size);
    ACR_REG_WR32(CSB, LW_CGSP_FALCON_SHA_IN_ADDR_HI, reg);


    // Set source config
    tmp = DRF_SIZE(LW_CGSP_FALCON_SHA_IN_ADDR_HI_MSB);
    tmp = ((LwU32)LwU64_HI32(msgAddr)) >> tmp;
    reg = DRF_NUM(_CGSP, _FALCON_SHA_SRC_CONFIG, _FB_BASE, tmp) |
          DRF_NUM(_CGSP, _FALCON_SHA_SRC_CONFIG, _CTXDMA, pTaskCfg->dmaIdx) |
          DRF_NUM(_CGSP, _FALCON_SHA_SRC_CONFIG, _SRC, pTaskCfg->srcType);
    ACR_REG_WR32(CSB, LW_CGSP_FALCON_SHA_SRC_CONFIG , reg);

    // Set task config
    reg = 0;
    if (pTaskCfg->defaultHashIv)
    {
        // sha512-224 ahs512-256, SW needs to set initial vector.
        if (algoId == SHA_ID_SHA_512_224 ||
            algoId == SHA_ID_SHA_512_256)
        {
            _shaSetInitVector_GA100(algoId);
            reg = FLD_SET_DRF(_CGSP_FALCON, _SHA_TASK_CONFIG, _HW_INIT_HASH,
                              _DISABLE, reg);
        }
        else
        {
            reg = FLD_SET_DRF(_CGSP_FALCON, _SHA_TASK_CONFIG, _HW_INIT_HASH,
                              _ENABLE, reg);
        }
    }
    else
    {
        reg = FLD_SET_DRF(_CGSP_FALCON, _SHA_TASK_CONFIG, _HW_INIT_HASH,
                          _DISABLE, reg);
    }
    ACR_REG_WR32(CSB, LW_CGSP_FALCON_SHA_TASK_CONFIG, reg);

    // Trigger OP start
    reg = 0;
    reg = FLD_SET_DRF(_CGSP_FALCON, _SHA_OPERATION, _LAST_BUF, _TRUE, reg);
    reg = FLD_SET_DRF(_CGSP_FALCON, _SHA_OPERATION, _OP, _START, reg);
    ACR_REG_WR32(CSB, LW_CGSP_FALCON_SHA_OPERATION, reg);

    status = shaWaitEngineIdle_HAL(pSha, SHA_ENGINE_IDLE_TIMEOUT);

    if (status != ACR_OK)
    {
        return status;
    }

    // Check if any error interrupt asserted
    reg = ACR_REG_RD32(CSB, LW_CGSP_FALCON_SHA_ERR_STATUS);

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

#else
    ct_assert(0);
#endif

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
ACR_STATUS
shaGetBlockSizeByte_GA100
(
   SHA_ID algoId,
   LwU32  *pBlockSizeByte
)
{
    switch (algoId)
    {
        case SHA_ID_SHA_1:
        case SHA_ID_SHA_224:
        case SHA_ID_SHA_256:
            {
                *pBlockSizeByte = SHA_256_BLOCK_SIZE_BYTE;
            }
        break;

        case SHA_ID_SHA_384:
        case SHA_ID_SHA_512:
        case SHA_ID_SHA_512_224:
        case SHA_ID_SHA_512_256:
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
static ACR_STATUS
_prepareHashFetchConfig_GA100
(
    SHA_ID   algoId,
    LwU32   *pLoopCount,
    LwBool  *pHashEntry64,
    LwBool  *pFetchHalfWord
)
{
    LwU32 i;

    *pHashEntry64 = LW_FALSE;
    *pFetchHalfWord = LW_FALSE;

    if (algoId == SHA_ID_SHA_1)
    {
        i = SHA_1_HASH_SIZE_BYTE >> 2;
    }
    else if (algoId == SHA_ID_SHA_224)
    {
        i = SHA_224_HASH_SIZE_BYTE >> 2;
    }
    else if (algoId == SHA_ID_SHA_256)
    {
        i = SHA_256_HASH_SIZE_BYTE >> 2;
    }
    else if (algoId == SHA_ID_SHA_384)
    {
        i =  SHA_384_HASH_SIZE_BYTE >> 2;
        *pHashEntry64 = LW_TRUE;
    }
    else if (algoId == SHA_ID_SHA_512)
    {
        i = SHA_512_HASH_SIZE_BYTE >> 2;
        *pHashEntry64 = LW_TRUE;
    }
    else if (algoId == SHA_ID_SHA_512_224)
    {
        i = SHA_512_224_HASH_SIZE_BYTE >> 2;
        *pHashEntry64 = LW_TRUE;
        *pFetchHalfWord = LW_TRUE;
    }
    else if (algoId == SHA_ID_SHA_512_256)
    {
        i = SHA_512_256_HASH_SIZE_BYTE >> 2;
        *pHashEntry64 = LW_TRUE;
    }
    else
    {
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
ACR_STATUS
shaReadHashResult_GA100
(
    SHA_CONTEXT *pShaContext,
    void *pBuf,
    LwBool bScrubReg
)
{
    ACR_STATUS status;
    LwU32  i, regIndex, v1, v2;
    LwU32  copyNextDword = LW_TRUE;
    SHA_ID algoId;
    LwU32  loopCount;
    LwBool bHashEntry64;
    LwBool bFetchHalfWord;
    LwU8  *pDest = (LwU8 *)pBuf;
    LwU8  *pSrc1 = (LwU8 *)&v1;
    LwU8  *pSrc2 = (LwU8 *)&v2;

    if (pShaContext == NULL ||
        pBuf == NULL)
    {
        return  ACR_ERROR_ILWALID_ARGUMENT;
    }

    algoId = pShaContext->algoId;

    status = _prepareHashFetchConfig_GA100(algoId, &loopCount, &bHashEntry64, &bFetchHalfWord);

    if (status != ACR_OK)
    {
        return status;
    }

#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)

    for (i = 0, regIndex = 0 ; i < loopCount; i++)
    {
        v1 = ACR_REG_RD32(CSB, LW_CSEC_FALCON_SHA_HASH_RESULT(regIndex));
        if (bScrubReg)
        {
            ACR_REG_WR32(CSB, LW_CSEC_FALCON_SHA_HASH_RESULT(regIndex), 0);
        }
        regIndex++;

        // SHA engine save hash data with big-endian format; we need to transfer to little-endian format
        v1 = LW_BYTESWAP32(v1);

        if (bHashEntry64)
        {
            v2 = ACR_REG_RD32(CSB, LW_CSEC_FALCON_SHA_HASH_RESULT(regIndex));
            if (bScrubReg)
            {
               ACR_REG_WR32(CSB, LW_CSEC_FALCON_SHA_HASH_RESULT(regIndex), 0);
            }

            v2 = LW_BYTESWAP32(v2);
            regIndex ++;

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

#elif defined(ASB)

    for (i = 0, regIndex = 0 ; i < loopCount; i++)
    {
        v1 = ACR_REG_RD32(CSB, LW_CGSP_FALCON_SHA_HASH_RESULT(regIndex));
        if (bScrubReg)
        {
            ACR_REG_WR32(CSB, LW_CGSP_FALCON_SHA_HASH_RESULT(regIndex), 0);
        }
        regIndex++;

        // SHA engine save hash data with big-endian format; we need to transfer to little-endian format
        v1 = LW_BYTESWAP32(v1);

        if (bHashEntry64)
        {
            v2 = ACR_REG_RD32(CSB, LW_CGSP_FALCON_SHA_HASH_RESULT(regIndex));
            if (bScrubReg)
            {
               ACR_REG_WR32(CSB, LW_CGSP_FALCON_SHA_HASH_RESULT(regIndex), 0);
            }

            v2 = LW_BYTESWAP32(v2);
            regIndex ++;

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

#else
    ct_assert(0);
#endif

}
