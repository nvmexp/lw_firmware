/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* Most of the steps are borrowed from : https://rmopengrok.lwpu.com/source/s?refs=verifyRSAPSSSignature&project=sw */

#include "rv_acr.h"

#include "acr_rv_verifkeyst234.h"

#include <liblwriscv/libc.h>
#include <liblwriscv/io_pri.h>
#include <liblwriscv/sha.h>

#include "tegra_se.h"
#include "tegra_lwpka_rsa.h"
#include "tegra_lwpka_gen.h"
#include "error_code.h"

#include <dev_pwr_pri.h>

static void acrSwapEndianness(void* pOutData, void* pInData, const uint32_t size)
{
    uint32_t i;

    for (i = 0; i < size / 2; i++)
    {
        uint8_t b1 = ((uint8_t*)pInData)[i];
        uint8_t b2 = ((uint8_t*)pInData)[size - 1 - i];
        ((uint8_t*)pOutData)[i]            = b2;
        ((uint8_t*)pOutData)[size - 1 - i] = b1;
    }
}

static ACR_STATUS maskGenerationFunctionSHA256(uint8_t *result, uint8_t *seed, uint32_t len)
{
    uint32_t i, outlen = 0;
    uint8_t hash[RSASSA_PSS_HASH_LENGTH_BYTE] = {0};
    SHA_TASK_CONFIG shaTaskConfig = {0};
    SHA_CONTEXT shaCtx = {0};
    LWRV_STATUS status = LWRV_OK;

    for ( i = 0; outlen < len; i++ ) {
        seed[RSASSA_PSS_HASH_LENGTH_BYTE + 0U] = (uint8_t)((i >> 24U) & 255U);
        seed[RSASSA_PSS_HASH_LENGTH_BYTE + 1U] = (uint8_t)((i >> 16U) & 255U);
        seed[RSASSA_PSS_HASH_LENGTH_BYTE + 2U] = (uint8_t)((i >> 8U) & 255U);
        seed[RSASSA_PSS_HASH_LENGTH_BYTE + 3U] = (uint8_t)(i & 255U);

        shaTaskConfig.srcType = 0; //DMEM
        shaTaskConfig.bDefaultHashIv = LW_TRUE;
        shaTaskConfig.dmaIdx = 0;
        shaTaskConfig.size = RSASSA_PSS_HASH_LENGTH_BYTE+4;
        shaTaskConfig.addr = (LwU64)seed;

        shaCtx.algoId = SHA_ALGO_ID_SHA_256;
        shaCtx.bufSize = RSASSA_PSS_HASH_LENGTH_BYTE;
        shaCtx.msgSize = RSASSA_PSS_HASH_LENGTH_BYTE + 4U;
        shaCtx.mutexToken = LSF_FALCON_ID_GSP_RISCV;
        shaCtx.pBufOut = (LwU8*)&hash;

        status = shaRunSingleTaskCommon(&shaTaskConfig, &shaCtx);
        if (status != LWRV_OK)
        {
            return ACR_ERROR_LS_SIG_VERIF_FAIL;
        }

        if ( (outlen + RSASSA_PSS_HASH_LENGTH_BYTE) <= len )
        {
            memcpy( (void *)(result + outlen), (void *)hash,
                                    RSASSA_PSS_HASH_LENGTH_BYTE);
            outlen += RSASSA_PSS_HASH_LENGTH_BYTE;
        }
        else
        {
            memcpy( (void *)(result + outlen), (void *)hash, len - outlen );
            outlen = len;
        }
    }

    return status;
}

ACR_STATUS acrValidateSignature
(
    uint8_t *sha256_hash,
    uint8_t *signature
)
{
    status_t                     ret                         = 0;
    engine_t                     *engine                     = NULL;

    ret = ccc_select_engine((const struct engine_s **)&engine,
                            ENGINE_CLASS_RSA,
                            ENGINE_PKA);
    if (ret != NO_ERROR)
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    ret = lwpka_acquire_mutex(engine);
    if (ret != NO_ERROR)
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    struct se_data_params        input                                      = {0};
    struct se_engine_rsa_context rsa_econtext                               = {0};
    unsigned char                result[RSA3K_SIGNATURE_PADDED_SIZE_BYTE]   = {0};

    /* Fill in se_data_params required by CCC */
    input.input_size  = RSA3K_SIGNATURE_SIZE_BYTE;
    input.output_size = RSA3K_SIGNATURE_SIZE_BYTE;
	input.src = (uint8_t *)signature; /* Char string to byte vector */
	input.dst = (uint8_t *)result;    /* Char string to byte vector */
    
    /* Fill in se_engine_rsa_context required by CCC */
    rsa_econtext.engine = engine;
    rsa_econtext.rsa_size = RSA3K_SIGNATURE_SIZE_BYTE;
    rsa_econtext.rsa_flags = RSA_FLAGS_DYNAMIC_KEYSLOT;
    rsa_econtext.rsa_keytype = KEY_TYPE_RSA_PUBLIC;

    if (acrIsDebugModeEnabled_HAL()) 
    {
    	rsa_econtext.rsa_pub_exponent = (uint8_t *)dbg_exp;
    	rsa_econtext.rsa_pub_expsize = sizeof(dbg_exp);
    	rsa_econtext.rsa_modulus = (uint8_t *)dbg_modulus;
    } else {
    	rsa_econtext.rsa_pub_exponent = (uint8_t *)prod_exp;
    	rsa_econtext.rsa_pub_expsize = sizeof(prod_exp);
    	rsa_econtext.rsa_modulus = (uint8_t *)prod_modulus;
    }
    /* LS signature input, modulus and the exponent are all in little endian. */
    ret = engine_lwpka_rsa_modular_exp_locked(&input, &rsa_econtext);

    lwpka_release_mutex(engine);

    if (ret != NO_ERROR)
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    /* Colwert little endian response from LWPKA mod-exp to big endian */
    acrSwapEndianness(result, result, RSA3K_SIGNATURE_SIZE_BYTE);

    uint32_t rsaSigSize  = RSA3K_SIGNATURE_SIZE_BITS;
    uint8_t  *EM         = result;
    uint8_t  *mHash      = sha256_hash;
    uint32_t i           = 0;

    uint8_t  H[RSASSA_PSS_HASH_LENGTH_BYTE + 4]                           = {0}; /* Buffer of H + 4 extra bytes (highest 4) for padding counter in MGF */
    uint8_t  H_[RSASSA_PSS_HASH_LENGTH_BYTE]                              = {0};
    uint8_t  M_[RSASSA_PSS_HASH_LENGTH_BYTE + RSASSA_PSS_SALT_LENGTH_BYTE + 8] = {0}; /* M' = 00_00_00_00_00_00_00_00 || mHash || salt */
    uint8_t  maskedDB[RSA3K_SIGNATURE_PADDED_SIZE_BYTE]                   = {0}; /* Support up to RSA4K */
    uint8_t  dbmask[RSA3K_SIGNATURE_PADDED_SIZE_BYTE]                     = {0};

    uint32_t emBits                 = rsaSigSize - 1;
    uint32_t emLen                  = RSA3K_SIGNATURE_SIZE_BYTE;
    uint32_t maskedDBLen            = emLen - RSASSA_PSS_HASH_LENGTH_BYTE - 1;
    uint32_t leftMostEmptyDBMask    = (uint32_t)(0xff >> (8*emLen-emBits));  /* Big Endian */

    if (rsaSigSize != RSA3K_SIGNATURE_SIZE_BITS)
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    if (EM[emLen-1] != 0xbc)
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    /* Extract H from EM */
    memcpy((void *)(H), (void *)(EM + (emLen - 1 - RSASSA_PSS_HASH_LENGTH_BYTE)), RSASSA_PSS_HASH_LENGTH_BYTE);

    /* Extract maskedDB from EM */
    memcpy((void *)maskedDB, (void *)EM, maskedDBLen);

    /* 8*emLen-emBits bit in big endian is zero */
    if (maskedDB[0] & ~leftMostEmptyDBMask)
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    /* Generate dbmask via mask generation function */
    maskGenerationFunctionSHA256(dbmask, H, maskedDBLen);

    /* DB = dbmask xor maskedDB */
    for (i = 0; i < maskedDBLen; i++)
    {
        maskedDB[i] = (dbmask[i] ^ maskedDB[i]);
    }

    /* Clear leftmost 8*emLen-emBits bits of DB to 0 */
    maskedDB[0] = (uint8_t)(maskedDB[0] & leftMostEmptyDBMask );

    /* Check if DB = PS || 0x1 || salt, PS are all 0s
       Extract salt from DB */
    memcpy((void *)(M_ + 8U + RSASSA_PSS_HASH_LENGTH_BYTE),
        (void *)(maskedDB + emLen - 1 - RSASSA_PSS_HASH_LENGTH_BYTE - RSASSA_PSS_SALT_LENGTH_BYTE), RSASSA_PSS_SALT_LENGTH_BYTE);

    if (maskedDB[emLen - 2 - RSASSA_PSS_HASH_LENGTH_BYTE - RSASSA_PSS_SALT_LENGTH_BYTE] != 0x1)
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    for (i = 0; i < maskedDBLen-RSASSA_PSS_SALT_LENGTH_BYTE-1; i++)
    {
        if (maskedDB[i] != 0U)    /* PS should be 00s */
        {
            return ACR_ERROR_LS_SIG_VERIF_FAIL;
        }
    }

    memcpy((void *)(M_ + 8U), (void *)mHash, RSASSA_PSS_HASH_LENGTH_BYTE);

    SHA_TASK_CONFIG shaTaskConfig = {0};
    shaTaskConfig.srcType = 0; //DMEM
    shaTaskConfig.bDefaultHashIv = LW_TRUE;
    shaTaskConfig.dmaIdx = 0;
    shaTaskConfig.size = RSASSA_PSS_HASH_LENGTH_BYTE +
                                            RSASSA_PSS_SALT_LENGTH_BYTE + 8;
    shaTaskConfig.addr = (LwU64)M_;

    SHA_CONTEXT shaCtx = {0};
    shaCtx.algoId = SHA_ALGO_ID_SHA_256;
    shaCtx.bufSize = RSASSA_PSS_HASH_LENGTH_BYTE;
    shaCtx.msgSize = RSASSA_PSS_HASH_LENGTH_BYTE + RSASSA_PSS_SALT_LENGTH_BYTE + 8;
    shaCtx.mutexToken = LSF_FALCON_ID_GSP_RISCV;
    shaCtx.pBufOut = (LwU8*)&H_;

    LWRV_STATUS status = shaRunSingleTaskCommon(&shaTaskConfig, &shaCtx);
    if (status != LWRV_OK)
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    /* Check if H == H';  H' is in sha256_value */
    if (memcmp((void *)(H), (void *)&H_[0], RSASSA_PSS_HASH_LENGTH_BYTE) != 0U)
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    return ACR_OK;

}

/*!
 * @brief Plain text hash generation for ID = LS_SIG_VALIDATION_ID_1 (Binary data + ucodeVer + ucodeId + Dep Map Context)
 *
   ---------|=================|-- binOffset
   |        |                 | |
   |        |     Bin data    | |
   |        |      (FB)       | |
   |        |                 | |
   |        |                 | \
   |        |                 |    => SHA operation : Task -1
binSize     |                 | /
   |        |                 | |
   |        |                 | |
   |        |                 | |
   |        |                 | |
  ----------|=================| --

   |==================| ---
   |    falconId      |  |
   |     (DMEM)       |  |
   |==================|  |
   |    ucodeVer      |  |
   |     (DMEM)       |  \
   |==================|    => SHA operation : Task-2
   |     ucodeId      |  /
   |     (DMEM)       |  |
   |==================|  |
   | Dep Map Context  |  |
   |     (DMEM)       |  |
   |==================| ---

 */
static ACR_STATUS acrGeneratePlainTextHash
(
    LwU32              binarysize,
    LwU32              binOffset,
    PLSF_UCODE_DESC_V2 pUcodeDesc,
    LwU8*              pMessageHash
)
{
    status_t        ret           = 0;
    SHA_TASK_CONFIG shaTaskConfig = {0};
    LwU32           plaintextSize = binarysize + 12 + (pUcodeDesc->depMapCount * 8);
    LwU64           binStartAddr;
    SHA_CONTEXT     shaCtx;

    // The copybuf size was initially decided using depMapCount value
    // But SSP doesn't handle this well and throws stack protection warnings
    // for local variable buffer assigned, plus having variable local buffer is a
    // safety concern as well. Thus, created a static buffer such that copybuf can
    // allocate 64 bytes of pUcodeDesc->depMap value.
    // Lwrrently depMapCount is 0 for ACR
    LwU32           copybuf[(12 + (6 * 8))/4] = {0};

    binStartAddr = g_dmaProp.wprBase + (LwU64)binOffset;

    shaCtx.algoId  = SHA_ALGO_ID_SHA_256;
    shaCtx.msgSize = plaintextSize;
    shaCtx.pBufOut = pMessageHash;
    shaCtx.bufSize = SHA_256_HASH_SIZE_BYTE;
    shaCtx.mutexToken = LSF_FALCON_ID_GSP_RISCV;

    ret = shaAcquireMutex(LSF_FALCON_ID_GSP_RISCV);
    if (ret != LWRV_OK) return ACR_ERROR_LS_SIG_VERIF_FAIL;

    // Task-1: SHA hash callwlation of bin data stored at FB
    ret = shaOperationInit(&shaCtx);
    if (ret != LWRV_OK) return ACR_ERROR_LS_SIG_VERIF_FAIL;

    shaTaskConfig.srcType = 2; //FB
    shaTaskConfig.bDefaultHashIv = LW_TRUE;
    shaTaskConfig.dmaIdx = g_dmaProp.ctxDma;
    shaTaskConfig.size = binarysize;
    shaTaskConfig.addr = binStartAddr;

    ret = shaInsertTask(&shaCtx, &shaTaskConfig);
    if (ret != LWRV_OK) return ACR_ERROR_LS_SIG_VERIF_FAIL;

    //
    // Task-2 : Leverage copybuf as temp buffer to save concatenation components
    // bin || falconId || lsUcodeVer || lsUocdeId || depMapCtx
    //
    copybuf[0] = pUcodeDesc->falconId;
    copybuf[1] = pUcodeDesc->lsUcodeVersion;
    copybuf[2] = pUcodeDesc->lsUcodeId;
    memcpy( (void *)&copybuf[3], (void *)pUcodeDesc->depMap, (pUcodeDesc->depMapCount * 8));

    shaTaskConfig.srcType = 0; //DMEM
    shaTaskConfig.bDefaultHashIv = LW_FALSE;
    shaTaskConfig.dmaIdx = 0;
    shaTaskConfig.size = 12 + (pUcodeDesc->depMapCount * 8);
    shaTaskConfig.addr = (LwU64)(copybuf);

    ret = shaInsertTask(&shaCtx, &shaTaskConfig);
    if (ret != LWRV_OK) return ACR_ERROR_LS_SIG_VERIF_FAIL;

    ret = shaReadHashResult(&shaCtx, LW_TRUE);
    if (ret != LWRV_OK) return ACR_ERROR_LS_SIG_VERIF_FAIL;

    ret = shaReleaseMutex(LSF_FALCON_ID_GSP_RISCV);
    if (ret != LWRV_OK) return ACR_ERROR_LS_SIG_VERIF_FAIL;

    return ret;
}

/*!
 *@brief To validate LS signature with PKC-LS support and new wpr blob defines
 */
ACR_STATUS acrVerifySignature_T234
(
    LwU8               *pSignature,
    LwU32              binarysize,
    LwU32              binOffset,
    PLSF_UCODE_DESC_V2 pUcodeDesc
)
{
    ACR_STATUS      status = ACR_OK;
    LwU8            messageHash[SHA_256_HASH_SIZE_BYTE];

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGeneratePlainTextHash(binarysize, binOffset, pUcodeDesc, messageHash));

    status = acrValidateSignature(messageHash, pSignature);

    return status;
}
