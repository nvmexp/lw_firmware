/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* Most of the steps are borrowed from : https://rmopengrok.lwpu.com/source/s?refs=verifyRSAPSSSignature&project=sw */

#include <liblwriscv/libc.h>
#include <liblwriscv/print.h>

#include "tegra_se.h"
#include "tegra_lwpka_rsa.h"
#include "tegra_lwpka_gen.h"
#include "sha256.h"
#include "utils.h"
#include "rsa3kpss.h"

#define LW_CEIL(a,b)                    (((a)+(b)-1)/(b))
#define RSASSA_PSS_HASH_LENGTH          32
#define RSASSA_PSS_SALT_LENGTH          RSASSA_PSS_HASH_LENGTH
#define CRYPTO_RSA4K_SIG_SIZE           512
#define CRYPTO_RSA3K_SIG_SIZE           384
#define CRYPTO_RSA3K_SIG_BITS_SIZE      8 * CRYPTO_RSA3K_SIG_SIZE

static void MGF1(uint8_t *result, uint8_t *seed, uint32_t len)
{
    uint32_t i, outlen = 0;
    uint8_t hash[RSASSA_PSS_HASH_LENGTH] = {0};
    SHA256_CTX shaCtx = {0};

    for ( i = 0; outlen < len; i++ ) {
        seed[32] = (uint8_t)((i >> 24) & 255);
        seed[33] = (uint8_t)((i >> 16) & 255);
        seed[34] = (uint8_t)((i >> 8)) & 255;
        seed[35] = (uint8_t)(i & 255);

        // TODO: Replace with HW SHA library once it's available in liblwriscv
        sha256_init(&shaCtx);
        sha256_update(&shaCtx, seed, (RSASSA_PSS_HASH_LENGTH+4));
        sha256_final(&shaCtx, hash);

        if ( (outlen + RSASSA_PSS_HASH_LENGTH) <= len )
        {
            memcpy( (void *)(result + outlen), (void *)hash, RSASSA_PSS_HASH_LENGTH );
            outlen += RSASSA_PSS_HASH_LENGTH;
        }
        else
        {
            memcpy( (void *)(result + outlen), (void *)hash, len - outlen );
            outlen = len;
        }
    }
}

int rsa3kPssVerify(void)
{
    uint32_t                     data                        = 0;
    engine_t                     *engine                     = NULL;
    status_t                     ret                         = 0;
    struct se_data_params        input                       = {0};
    struct se_engine_rsa_context rsa_econtext                = {0};

    printf ("[START] CheetAh select engine\n");
    ret = tegra_select_engine((const struct engine_s **)&engine, ENGINE_CLASS_RSA, ENGINE_PKA);
    printf ("[END] CheetAh select engine = 0x%x\n", ret);

#ifdef TEST_MUTEX
    printf ("[START] Acquire mutex\n");
    ret = lwpka_acquire_mutex(engine);
    printf ("[END] Acquire mutex = 0x%x\n", ret);
#endif

    printf ("[START] Read mutex status\n");
#if defined SAMPLE_PROFILE_fsp
    data = readWrapper((volatile void *)0x0151011c);
#else
    data = dioSeReadWrapper((volatile void *)0x0000011c);
#endif
    printf ("[END] Mutex status = 0x%x\n", data);


// TODO: For some reason, defining this at the top initializes all variables to 0.
// Probably something to do with global variable initialization issues in sample app.
#include "test-vector-rsa3kpss.h"

    unsigned char result[RSA3072_BYTE_SIZE] = {0};

    // TODO: CCC does not colwert this Big Endian data to Little Endian. Need to confirm with Juki.
    // CCC code shows a TODO for RSA_FLAGS_BIG_ENDIAN_DATA, so probably because of that?
    if (flags & BIG_ENDIAN_SIGNATURE)
    {
        swapEndianness(&signature, &signature, RSA3072_BYTE_SIZE);
    }

    /* Fill in se_data_params required by CCC */
    input.input_size  = RSA3072_BYTE_SIZE;
    input.output_size = RSA3072_BYTE_SIZE;
	input.src = (uint8_t *)signature; /* Char string to byte vector */
	input.dst = (uint8_t *)result;    /* Char string to byte vector */

    /* Fill in se_engine_rsa_context required by CCC */
    rsa_econtext.engine = engine;
    rsa_econtext.rsa_size = RSA3072_BYTE_SIZE;
    rsa_econtext.rsa_flags = RSA_FLAGS_DYNAMIC_KEYSLOT;
    rsa_econtext.rsa_flags |= (flags & BIG_ENDIAN_EXPONENT)? RSA_FLAGS_BIG_ENDIAN_EXPONENT : 0; /* CCC will take care of endian colwersion */
    rsa_econtext.rsa_flags |= (flags & BIG_ENDIAN_MODULUS)? RSA_FLAGS_BIG_ENDIAN_MODULUS : 0;   /* CCC will take care of endian colwersion */
    rsa_econtext.rsa_keytype = KEY_TYPE_RSA_PUBLIC;
    rsa_econtext.rsa_pub_exponent = (uint8_t *)pub_exp;
    rsa_econtext.rsa_pub_expsize = sizeof(pub_exp);
    rsa_econtext.rsa_modulus = (uint8_t *)modulus;

    printf ("[START] RSA mod-exp operation\n");
    ret = engine_lwpka_rsa_modular_exp_locked(&input, &rsa_econtext);
    printf ("[END] RSA mod-exp operation = 0x%x\n", ret);

    /* Need to colwert big endian response from LWPKA  mod-exp to little endian */
    swapEndianness(&result, &result, RSA3072_BYTE_SIZE);

#ifdef TEST_MUTEX
    printf ("[START] Release mutex\n");
    lwpka_release_mutex(engine);
    printf ("[END] Release mutex\n");
#endif

    /* The SHA256 library we use expects the hash in little endian format */
    if (flags & BIG_ENDIAN_SHA256HASH)
    {
        swapEndianness(sha256_hash, sha256_hash, SHA256_BYTE_SIZE);
    }

    uint32_t rsaSigSize  = CRYPTO_RSA3K_SIG_BITS_SIZE;
    uint8_t  *EM         = result;
    uint8_t  *mHash      = sha256_hash;
    uint32_t i           = 0;

    uint8_t  H[RSASSA_PSS_HASH_LENGTH + 4]                           = {0}; /* Buffer of H + 4 extra bytes for padding counter in MGF */
    uint8_t  H_[RSASSA_PSS_HASH_LENGTH]                              = {0};
    uint8_t  M_[RSASSA_PSS_HASH_LENGTH + RSASSA_PSS_SALT_LENGTH + 8] = {0}; /* M' = 00_00_00_00_00_00_00_00 || mHash || salt */
    uint8_t  maskedDB[CRYPTO_RSA4K_SIG_SIZE]                         = {0}; /* Support up to RSA4K */
    uint8_t  dbmask[CRYPTO_RSA4K_SIG_SIZE]                           = {0};

    uint32_t emBits                 = rsaSigSize - 1;
    uint32_t emLen                  = LW_CEIL(emBits, 8);
    uint32_t maskedDBLen            = emLen - RSASSA_PSS_HASH_LENGTH - 1;
    uint32_t leftMostEmptyDBMask    = (uint32_t)(0xff >> (8*emLen-emBits));  /* Big Endian */

    printf ("[START] RSA3KPSS signature validation\n");
    if (rsaSigSize != CRYPTO_RSA3K_SIG_BITS_SIZE)
    {
        printf("[ERROR] RSAPSS SIGNATURE SIZE UNSUPPORTED\n");
        return 1;
    }

    if (EM[emLen-1] != 0xbc)
    {
        printf("[ERROR] RSAPSS EM ANCHOR CHECK FAILED\n");
        return 1;
    }

    /* Extract H from EM, lowest 4 bytes is for counter in MGF */
    for (i = 0; i < RSASSA_PSS_HASH_LENGTH; i++)
    {
        H[i] = EM[emLen - 1 - RSASSA_PSS_HASH_LENGTH + i];
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
        return 1;
    }

    /* Generate dbmask via MGF function */
    MGF1(dbmask, H, maskedDBLen);

    /* DB = dbmask xor maskedDB */
    for (i = 0; i < maskedDBLen; i++)
    {
        maskedDB[i] = (dbmask[i] ^ maskedDB[i]);
    }

    /* Clear leftmost 8*emLen-emBits bits of DB to 0 */
    maskedDB[0] = (uint8_t)(maskedDB[0] & leftMostEmptyDBMask );

    /* Check if DB = PS || 0x1 || salt, PS are all 0s
       Extract salt from DB */
    for (i = 0; i < RSASSA_PSS_SALT_LENGTH; i++)
    {
        M_[8+ RSASSA_PSS_HASH_LENGTH + i] = maskedDB[emLen - 1 - RSASSA_PSS_HASH_LENGTH - RSASSA_PSS_SALT_LENGTH + i];
    }

    if (maskedDB[emLen - 2 - RSASSA_PSS_HASH_LENGTH - RSASSA_PSS_SALT_LENGTH] != 0x1)
    {
        printf("[ERROR] RSAPSS MASKDB SALT MARK ERROR\n");
        return 1;
    }

    for (i = 0; i < maskedDBLen-RSASSA_PSS_SALT_LENGTH-1; i++)
    {
        if (maskedDB[i])    /* PS should be 00s */
        {
            printf("[ERROR] RSAPSS MASKDB PS ERROR\n");
            return 1;

        }
    }

    /* M' = 00_00_00_00_00_00_00_00 || mHash || salt */
    for (i = 0; i < 8; i++)
    {
        M_[i] = 0;
    }

    for (i = 0; i < RSASSA_PSS_HASH_LENGTH; i++)
    {
        M_[8+ i] = ( (unsigned char *) mHash)[i];
    }

    // TODO: Replace with HW SHA library once it's available in liblwriscv
    SHA256_CTX shaCtx;
    sha256_init(&shaCtx);
    sha256_update(&shaCtx, M_, (RSASSA_PSS_HASH_LENGTH + RSASSA_PSS_SALT_LENGTH + 8));
    sha256_final(&shaCtx, H_);

    /* Check if H == H';  H' is in sha256_value */
    if (memCompare( (void *)(H), (void *)&H_[0], RSASSA_PSS_HASH_LENGTH))
    {
        printf("[ERROR] RSAPSS SIG VERIFY FAILED\n");
        return 1;
    }

    printf("[END] RSA3KPSS signature validation sucesssful\n");
    return 0;
}
