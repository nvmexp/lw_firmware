/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <liblwriscv/libc.h>
#include <liblwriscv/print.h>

#include "tegra_se.h"
#include "tegra_lwpka_gen.h"

#include "sha.h"

/*!
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 * This code is for testing and libCCC usage purposes only!
 * Do not replicate blindly for production usecases. Consult with your local crypto expert!
 * 
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 * */

#define SHA2_256_SIZE_BYTES     32U
#define SHA2_256_BLOCK_BYTES    64U

#define SHA2_384_SIZE_BYTES     48U
#define SHA2_384_BLOCK_BYTES    128U

#define MAX_SHA_SIZE_BYTES      SHA2_384_SIZE_BYTES

#define MESSAGE_LENGTH          174U

static void verifyHashCommon
(
    te_crypto_algo_t shaAlgo,
    uint8_t *expected_hash
)
{
    uint32_t                           i                        = 0;
    uint32_t                           match                    = 0;
    uint32_t                           hashBlockSize            = 0;
    uint32_t                           hashSize                 = 0;
    status_t                           ret                      = 0;
    engine_t                           *engine                  = NULL;
    struct se_data_params              input_params             = {0};
    struct se_engine_sha_context       econtext                 = {0};
    uint8_t                            dst[MAX_SHA_SIZE_BYTES]  = {0};

    char    src[MESSAGE_LENGTH]      = "This is to test whether SHA2-384 is working with big strings as well. Took me quite some time to come up with this string, which is strange, as it sounds so easy in practice.";

    if (shaAlgo == TE_ALG_SHA256) {
        hashBlockSize   = SHA2_256_BLOCK_BYTES;
        hashSize        = SHA2_256_SIZE_BYTES;
    } else {
        hashBlockSize   = SHA2_384_BLOCK_BYTES;
        hashSize        = SHA2_384_SIZE_BYTES;
    }

    printf ("[START] Select engine\n");
    ret = ccc_select_engine((const struct engine_s **)&engine, ENGINE_CLASS_SHA, ENGINE_SE0_SHA);
    printf ("[END] Select engine = 0x%x\n", ret);

    econtext.engine         = engine;
    econtext.is_first       = true;
	econtext.is_last        = true;
    econtext.algorithm      = shaAlgo;
    econtext.hash_algorithm = shaAlgo;
    econtext.sha_flags      = SHA_FLAGS_NONE;
    econtext.block_size     = hashBlockSize;
    econtext.hash_size      = hashSize;

	input_params.src            = (uint8_t *)src;
	input_params.input_size     = sizeof(src);
	input_params.dst            = (uint8_t *)dst;
	input_params.output_size    = hashSize;

    printf ("[START] Sha process block\n");
    ret = engine_sha_process_block_locked(&input_params, &econtext);
    printf ("[END] Sha process block = 0x%x\n", ret);

    printf("Expected hash value : ");
    for (i=0; i<hashSize; i++) {
        printf ("%02x", dst[i]);
    }
    printf("\n");

    printf("Generated hash value : ");
    for (i=0; i<hashSize; i++) {
        printf ("%02x", dst[i]);
        if (expected_hash[i] != dst[i]) {
            goto ErrorExit;
        }
    }
    printf("\n");
    match = 1;

ErrorExit:
    if (match) {
        printf("[END] Generated value matches the expected value!\n");
    } else {
        printf("[ERROR] Generated value does not match the expected value!\n");
    }
}

static void verifySHA256(void)
{
    uint8_t expected_hash[SHA2_256_SIZE_BYTES] = {
        0x17, 0x3e, 0xc6, 0x58, 0x45, 0xfb, 0x8e, 0xe2, 0x01, 0x62, 0x20, 0x3a, 0xbb, 0xe8, 0xa6, 0x39,
        0x66, 0xe7, 0x0b, 0x76, 0x42, 0x69, 0x4c, 0x72, 0xa9, 0x65, 0xa6, 0x3e, 0xb6, 0xbb, 0x39, 0x3c
    };

    verifyHashCommon(TE_ALG_SHA256, (uint8_t *)expected_hash);
}

static void verifySHA384(void)
{
    uint8_t expected_hash[SHA2_384_SIZE_BYTES] = { 
        0xa2, 0x80, 0xaf, 0xf7, 0xdd, 0xbc, 0xe6, 0xef, 0x2d, 0x8d, 0x70, 0x3c, 0x5c, 0x35, 0xbd, 0x7b,
        0x46, 0xf7, 0x57, 0x7c, 0xd9, 0xd8, 0xae, 0x12, 0x16, 0x9a, 0x3c, 0xe3, 0xfc, 0x39, 0x5d, 0xe6,
        0xbd, 0x6a, 0x12, 0xa5, 0x04, 0x1b, 0xbe, 0xd6, 0x64, 0x22, 0x50, 0xf5, 0x9b, 0x76, 0xc9, 0x01
    };

    verifyHashCommon(TE_ALG_SHA384, (uint8_t *)expected_hash);
}

void verifyHash(void)
{
    printf("\n* SHA2-256 *\n");
    verifySHA256();

    printf("\n* SHA2-384 *\n");
    verifySHA384();
}
