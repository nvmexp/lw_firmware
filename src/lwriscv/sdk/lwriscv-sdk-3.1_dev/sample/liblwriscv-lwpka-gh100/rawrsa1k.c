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
#include "rawrsa1k.h"

static void rawRsa1kVerifyCommon(void)
{
#include "test-vector-rawrsa1k.h"
    uint8_t                      *signature                  = (uint8_t*)&signature_arr;
    uint8_t                      *modulus                    = (uint8_t*)&modulus_arr;
    uint8_t                      *pub_exp                    = (uint8_t*)&pub_exp_arr;
    uint8_t                      pub_exp_size                = sizeof(pub_exp_arr);
    uint32_t                     data                        = 0;
    engine_t                     *engine                     = NULL;
    status_t                     ret                         = 0;
    struct se_data_params        input                       = {0};
    struct se_engine_rsa_context rsa_econtext                = {0};

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

    unsigned char result[CRYPTO_RSA1K_SIG_SIZE] = {0};

    // CCC does not colwert this Big Endian data to Little Endian, so we need to do it.
    if (flags & BIG_ENDIAN_SIGNATURE)
    {
        swapEndianness(signature, signature, CRYPTO_RSA1K_SIG_SIZE);
    }

    /* Fill in se_data_params required by CCC */
    input.input_size  = CRYPTO_RSA1K_SIG_SIZE;
    input.output_size = CRYPTO_RSA1K_SIG_SIZE;
	input.src = signature; /* Char string to byte vector */
	input.dst = (uint8_t *)result;    /* Char string to byte vector */
    
    /* Fill in se_engine_rsa_context required by CCC */
    rsa_econtext.engine = engine;
    rsa_econtext.rsa_size = CRYPTO_RSA1K_SIG_SIZE;
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

    /* Need to colwert little endian response from LWPKA mod-exp to big endian */
    swapEndianness(&result, &result, CRYPTO_RSA1K_SIG_SIZE);

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

    if (memCompare(result, expected_result, CRYPTO_RSA1K_SIG_SIZE))
    {
        printf("[ERROR] Result does not match expected value!\n");
        return;
    }
    printf("[END] Raw RSA 1k validation successful\n");
}


void rawRsa1kVerify(void)
{
    rawRsa1kVerifyCommon();
}
