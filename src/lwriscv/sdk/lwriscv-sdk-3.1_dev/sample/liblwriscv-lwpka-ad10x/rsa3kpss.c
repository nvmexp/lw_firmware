/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* Most of the steps are borrowed from : https://rmopengrok.lwpu.com/source/s?refs=verifyRSAPSSSignature&project=sw */

/*!
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 * This code is for testing and libCCC usage purposes only!
 * Do not replicate blindly for production usecases. Consult with your local crypto expert!
 * 
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 * */

#include <liblwriscv/libc.h>
#include <liblwriscv/print.h>

#include "tegra_se.h"
#include "tegra_lwpka_rsa.h"
#include "tegra_lwpka_gen.h"

#include "utils.h"
#include "rsa3kpss.h"
#include "peregrine_sha.h"
#include "test-vector-rsa3kpss-common.h"

static LwU32 shaRunSingleTaskCommon (SHA_ALGO_ID algoId, LwU32 sizeByte, SHA_TASK_CONFIG *pTaskCfg, LwU8 *pBufOut)
{
    LwU32 status = ACR_ERROR_SHA_ENG_ERROR;
    SHA_CONTEXT shaCtx;

    shaCtx.algoId  = algoId;
    shaCtx.msgSize = sizeByte;

    status = shaOperationInit(&shaCtx);
    status = shaInsertTask(&shaCtx, pTaskCfg);
    status = shaReadHashResult(&shaCtx, (void *)pBufOut, LW_TRUE);

    return status;
}

static void MGF1(uint8_t *result, uint8_t *seed, uint32_t len, SHA_ALGO_ID algoId)
{
    uint32_t i, outlen = 0;
    uint32_t hashSize  = 0;
    uint8_t hash[MAX_HASH_SIZE_BYTES] = {0};
    SHA_TASK_CONFIG shaTaskConfig = {0};

    if (algoId == SHA_ALGO_ID_SHA_256) {
        hashSize = 32;
    } else {
        hashSize = 48;
    }

    for ( i = 0; outlen < len; i++ ) {
        seed[hashSize]   = (uint8_t)((i >> 24) & 255);
        seed[hashSize+1] = (uint8_t)((i >> 16) & 255);
        seed[hashSize+2] = (uint8_t)((i >> 8)) & 255;
        seed[hashSize+3] = (uint8_t)(i & 255);

        // TODO: Replace with liblwriscv SHA APIs once it's available in liblwriscv
        shaTaskConfig.srcType = 0; //DMEM
        shaTaskConfig.bDefaultHashIv = LW_TRUE;
        shaTaskConfig.dmaIdx = 0;
        shaTaskConfig.size = hashSize+4;
        shaTaskConfig.addr = (LwU64)seed;
        shaRunSingleTaskCommon(algoId, hashSize+4, &shaTaskConfig, (LwU8*)&hash);

        if ( (outlen + hashSize) <= len )
        {
            memcpy( (void *)(result + outlen), (void *)hash, hashSize );
            outlen += hashSize;
        }
        else
        {
            memcpy( (void *)(result + outlen), (void *)hash, len - outlen );
            outlen = len;
        }
    }
}

static void rsa3kPssVerifyCommon
(
    uint8_t *signature,
    uint8_t *modulus,
    uint8_t *pub_exp,
    uint8_t pub_exp_size,
    uint8_t *hash,
    uint8_t flags,
    SHA_ALGO_ID algoId,
    uint8_t saltLen
)
{
    uint32_t                     data                        = 0;
    uint32_t                     hashSize                    = 0;
    engine_t                     *engine                     = NULL;
    status_t                     ret                         = 0;
    struct se_data_params        input                       = {0};
    struct se_engine_rsa_context rsa_econtext                = {0};

    if (algoId == SHA_ALGO_ID_SHA_256) {
        hashSize = 32;
    } else {
        hashSize = 48;
    }

    printf ("[START] Select engine\n");
    ret = ccc_select_engine((const struct engine_s **)&engine, ENGINE_CLASS_RSA, ENGINE_PKA);
    printf ("[END] Select engine = 0x%x\n", ret);

#ifdef TEST_MUTEX
    printf ("[START] Acquire mutex\n");
    ret = ACQUIRE_MUTEX();
    printf ("[END] Acquire mutex = 0x%x\n", ret);

    printf ("[START] Read mutex status\n");
#if defined LIBCCC_FSP_SE_ENABLED
    data = readWrapper((volatile void *)0x0151011c);
#else
    data = dioSeReadWrapper((volatile void *)0x0000011c);
#endif
    printf ("[END] Mutex status = 0x%x\n", data);
#endif

    unsigned char result[CRYPTO_RSA3K_SIG_SIZE] = {0};

    // CCC does not colwert this Big Endian data to Little Endian, so we need to do it.
    if (flags & BIG_ENDIAN_SIGNATURE)
    {
        swapEndianness(signature, signature, CRYPTO_RSA3K_SIG_SIZE);
    }

    /* Fill in se_data_params required by CCC */
    input.input_size  = CRYPTO_RSA3K_SIG_SIZE;
    input.output_size = CRYPTO_RSA3K_SIG_SIZE;
	input.src = signature; /* Char string to byte vector */
	input.dst = (uint8_t *)result;    /* Char string to byte vector */
    
    /* Fill in se_engine_rsa_context required by CCC */
    rsa_econtext.engine = engine;
    rsa_econtext.rsa_size = CRYPTO_RSA3K_SIG_SIZE;
    rsa_econtext.rsa_flags = RSA_FLAGS_DYNAMIC_KEYSLOT;
    rsa_econtext.rsa_flags |= (flags & BIG_ENDIAN_EXPONENT)? RSA_FLAGS_BIG_ENDIAN_EXPONENT : 0; /* CCC will take care of endian colwersion */
    rsa_econtext.rsa_flags |= (flags & BIG_ENDIAN_MODULUS)? RSA_FLAGS_BIG_ENDIAN_MODULUS : 0;   /* CCC will take care of endian colwersion */
    rsa_econtext.rsa_keytype = KEY_TYPE_RSA_PUBLIC;
    rsa_econtext.rsa_pub_exponent = pub_exp;
    rsa_econtext.rsa_pub_expsize = pub_exp_size;
    rsa_econtext.rsa_modulus = modulus;

    printf ("[START] RSA mod-exp operation\n");
    ret = engine_lwpka_rsa_modular_exp_locked(&input, &rsa_econtext);
    printf ("[END] RSA mod-exp operation = 0x%x\n", ret);
    CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(ret);

    /* Need to colwert big endian response from LWPKA  mod-exp to little endian */
    swapEndianness(&result, &result, CRYPTO_RSA3K_SIG_SIZE);

#ifdef TEST_MUTEX
    printf ("[START] Release mutex\n");
    RELEASE_MUTEX();
    printf ("[END] Release mutex\n");

    printf ("[START] Read mutex status\n");
#if defined LIBCCC_FSP_SE_ENABLED
    data = readWrapper((volatile void *)0x0151011c);
#else
    data = dioSeReadWrapper((volatile void *)0x0000011c);
#endif
    printf ("[END] Mutex status = 0x%x\n", data);
#endif

    uint32_t rsaSigSize  = CRYPTO_RSA3K_SIG_BITS_SIZE;
    uint8_t  *EM         = result;
    uint8_t  *mHash      = hash;
    uint32_t i           = 0;

    uint8_t  H[MAX_HASH_SIZE_BYTES + 4]                           = {0}; /* Buffer of H + 4 extra bytes for padding counter in MGF */
    uint8_t  H_[MAX_HASH_SIZE_BYTES]                              = {0};
    uint8_t  M_[MAX_HASH_SIZE_BYTES + MAX_SALT_LENGTH_BYTES + 8]  = {0}; /* M' = 00_00_00_00_00_00_00_00 || mHash || salt */
    uint8_t  maskedDB[CRYPTO_RSA4K_SIG_SIZE]                      = {0}; /* Support up to RSA4K */
    uint8_t  dbmask[CRYPTO_RSA4K_SIG_SIZE]                        = {0};

    uint32_t emBits                 = rsaSigSize - 1;
    uint32_t emLen                  = LW_CEIL(emBits, 8);
    uint32_t maskedDBLen            = emLen - hashSize - 1;
    uint32_t leftMostEmptyDBMask    = (uint32_t)(0xff >> (8*emLen-emBits));  /* Big Endian */

    printf ("[START] RSA3KPSS signature validation\n");
    if (rsaSigSize != CRYPTO_RSA3K_SIG_BITS_SIZE)
    {
        printf("[ERROR] RSAPSS SIGNATURE SIZE UNSUPPORTED\n");
        return;
    }

    if (EM[emLen-1] != 0xbc)
    {
        printf("[ERROR] RSAPSS EM ANCHOR CHECK FAILED\n");
        return;
    }

    /* Extract H from EM, lowest 4 bytes is for counter in MGF */
    for (i = 0; i < hashSize; i++)
    {
        H[i] = EM[emLen - 1 - hashSize + i];
    }

    /* Extract maskedDB from EM */
    for (i = 0; i < maskedDBLen; i++)
    {
        maskedDB[i] = EM[i];
    }

    /* 8*emLen-emBits bit in big endian is zero */
    if (maskedDB[0] & ~leftMostEmptyDBMask)
    {
        printf("[ERROR] RSAPSS MASKDB LEFT MARK ERROR\n");
        return;
    }

    /* Generate dbmask via MGF function */
    MGF1(dbmask, H, maskedDBLen, algoId);

    /* DB = dbmask xor maskedDB */
    for (i = 0; i < maskedDBLen; i++)
    {
        maskedDB[i] = (dbmask[i] ^ maskedDB[i]);
    }

    /* Clear leftmost 8*emLen-emBits bits of DB to 0 */
    maskedDB[0] = (uint8_t)(maskedDB[0] & leftMostEmptyDBMask );

    /* Check if DB = PS || 0x1 || salt, PS are all 0s
       Extract salt from DB */
    for (i = 0; i < saltLen; i++)
    {
        M_[8+ hashSize + i] = maskedDB[emLen - 1 - hashSize - saltLen + i];
    }

    if (maskedDB[emLen - 2 - hashSize - saltLen] != 0x1)
    {
        printf("[ERROR] RSAPSS MASKDB SALT MARK ERROR\n");
        return;
    }

    for (i = 0; i < maskedDBLen-saltLen-1; i++)
    {
        if (maskedDB[i])    /* PS should be 00s */
        {
            printf("[ERROR] RSAPSS MASKDB PS ERROR\n");
            return;

        }
    }

    /* M' = 00_00_00_00_00_00_00_00 || mHash || salt */
    for (i = 0; i < 8; i++)
    {
        M_[i] = 0;
    }

    for (i = 0; i < hashSize; i++)
    {
        M_[8+ i] = ( (unsigned char *) mHash)[i];
    }

    // TODO: Replace with liblwriscv SHA APIs once it's available in liblwriscv
    SHA_TASK_CONFIG shaTaskConfig = {0};
    shaTaskConfig.srcType = 0; //DMEM
    shaTaskConfig.bDefaultHashIv = LW_TRUE;
    shaTaskConfig.dmaIdx = 0;
    shaTaskConfig.size = hashSize + saltLen + 8;
    shaTaskConfig.addr = (LwU64)M_;
    shaRunSingleTaskCommon(algoId, hashSize + saltLen + 8, &shaTaskConfig, (LwU8*)&H_);

    /* Check if H == H';  H' is in sha_value */
    if (memCompare( (void *)(H), (void *)&H_[0], hashSize))
    {
        printf("[ERROR] RSAPSS SIG VERIFY FAILED\n");
        return;
    }

    printf("[END] RSA3KPSS signature validation sucesssful\n");
}

static void rsa3kPssSha256Verify(void)
{
#include "test-vector-rsa3kpss-sha256.h"
    rsa3kPssVerifyCommon((uint8_t*)&signature, (uint8_t*)&modulus, (uint8_t*)&pub_exp, sizeof(pub_exp), (uint8_t*)&hash, flags, SHA_ALGO_ID_SHA_256, saltLen);
}

static void rsa3kPssSha384Verify(void)
{
#include "test-vector-rsa3kpss-sha384.h"
    rsa3kPssVerifyCommon((uint8_t*)&signature, (uint8_t*)&modulus, (uint8_t*)&pub_exp, sizeof(pub_exp), (uint8_t*)&hash, flags, SHA_ALGO_ID_SHA_384, saltLen);
}

void rsa3kPssVerify(void)
{
    printf("\n* RSA3KPSS with SHA-256 *\n");
    rsa3kPssSha256Verify();

    printf("\n* RSA3KPSS with SHA-384 *\n");
    rsa3kPssSha384Verify();
}
