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
#include "tegra_lwpka_ec.h"
#include "tegra_lwpka_mod.h"
#include "crypto_ec.h"

#include "utils.h"
#include "ecdsa.h"

/*!
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 * This code is for testing and libCCC usage purposes only!
 * Do not replicate blindly for production usecases. Consult with your local crypto expert!
 * 
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 * */

#define MAX_SIZE_IN_BYTES 48 // Lwrrently max is P-384 lwrve

static void verifyEcdsaSigningCommon
(
    uint32_t algorithmByteSize,
    te_ec_lwrve_id_t lwrveId,
    uint8_t *msgHash,
    uint8_t *privateKey,
    uint8_t *randNum,
    uint8_t *R,
    uint8_t *S
)
{
    /* Not used lwrrently */
    //uint32_t    numAttempts    = 0;
    //uint32_t    retryLoopCount = 1000;

    uint32_t                     data                                = 0;
    uint8_t                      sigR[MAX_SIZE_IN_BYTES]             = {0};
    uint8_t                      sigS[MAX_SIZE_IN_BYTES]             = {0};
    uint8_t                      bigZero[MAX_SIZE_IN_BYTES]          = {0};
    uint8_t                      randNumIlwerse[MAX_SIZE_IN_BYTES]   = {0};
    struct se_data_params        input                               = {0};
    struct se_engine_ec_context  ec_context                          = {0};
    te_mod_params_t              mod_params                          = {0};
    engine_t                     *engine                             = NULL;
    status_t                     ret                                 = 0;
    const struct pka1_ec_lwrve_s *ec_lwrve                           = ccc_ec_get_lwrve(lwrveId);
    struct te_ec_point_s         point_P                             = {0};
    struct te_ec_point_s         point_Q                             = {0};

    printf ("[START] Select engine\n");
    ret = ccc_select_engine((const struct engine_s **)&engine, ENGINE_CLASS_EC, ENGINE_PKA);
    printf ("[END] Select engine = 0x%x\n", ret);

#ifdef TEST_MUTEX
    printf ("[START] Acquire mutex\n");
    ret = lwpka_acquire_mutex(engine);
    printf ("[END] Acquire mutex = 0x%x\n", ret);

    printf ("[START] Read mutex status\n");
#if defined LIBCCC_FSP_SE_ENABLED
    data = readWrapper((volatile void *)0x0151011c);
#else
    data = dioSeReadWrapper((volatile void *)0x0000011c);
#endif
    printf ("[END] Mutex status = 0x%x\n", data);
#endif

    /* Initialize params which are not going to change in the loop */
    mod_params.size     = algorithmByteSize;
    if (lwrveId == TE_LWRVE_NIST_P_256) {
        mod_params.op_mode  = PKA1_OP_MODE_ECC256;
    } else {
        mod_params.op_mode  = PKA1_OP_MODE_ECC384;
    }

    memcpy(point_P.x, ec_lwrve->g.gen_x, algorithmByteSize);
    memcpy(point_P.y, ec_lwrve->g.gen_y, algorithmByteSize);
    point_P.point_flags = CCC_EC_POINT_FLAG_NONE;

    input.point_P     = &point_P;
    input.input_size  = sizeof(point_P);
    input.point_Q     = &point_Q;
    input.output_size = sizeof(point_Q);

    ec_context.engine       = engine;
    ec_context.ec_flags     = CCC_EC_FLAGS_DYNAMIC_KEYSLOT;
    ec_context.ec_keytype   = KEY_TYPE_EC_PRIVATE;
    ec_context.ec_lwrve     = ec_lwrve;

    // TODO: Ideally we'd like to loop and generate random number as per the algorithm.
    //       However, since this is a test, we hardcode the random number and check
    //       the resultant R and S component of the signature with the expected result.

   /*
    for (numAttempts = 0; numAttempts < retryLoopCount; numAttempts++)
    {
        //
        // Generate a random number
        //
   */

        /* Check if random number is 0, if it is start all over */
        if (!memCompare(randNum, bigZero, algorithmByteSize))
        {
            //continue;
            printf("[ERROR] Provided random number should not be 0!\n");
            return;
        }

        /*
           sigR = X point of point multiplication operation
           This is step 2, part 2:  point multiplication of randNum * P(GenX, GenY), R = X1
        */
        memset(&point_Q, 0, sizeof(struct te_ec_point_s));
        ec_context.ec_k         = randNum;
        ec_context.ec_k_length  = algorithmByteSize;
        
        printf ("[START] EC point multiplication operation\n");
        ret = engine_lwpka_ec_point_mult_locked(&input, &ec_context);
        printf ("[END] EC point multiplication operation = 0x%x\n", ret);
        CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(ret);

        /*
           sigR = sigR mod gorder
           This is step 3:  R = X1 (from step 2.2) mod gOrder
        */
        mod_params.x = point_Q.x;
        mod_params.m = ec_context.ec_lwrve->n;
        mod_params.r = sigR;

        printf ("[START] Modular reduction operation\n");
        ret = engine_lwpka_modular_reduction_locked(engine, &mod_params);
        printf ("[END] Modular reduction operation = 0x%x\n", ret);
        CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(ret);

        /*
           Check if sigR is zero, if it is start over
        */
        if (!memCompare(sigR, bigZero, algorithmByteSize))
        {
            //continue;
            printf("[ERROR] SigR should not be 0!\n");
            return;
        }

        /*
           This is step 4, part 1:  d = privateKey mod gorder
        */
        mod_params.x = privateKey;
        mod_params.r = privateKey;

        printf ("[START] Modular reduction operation\n");
        ret = engine_lwpka_modular_reduction_locked(engine, &mod_params);
        printf ("[END] Modular reduction operation = 0x%x\n", ret);
        CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(ret);

        /*
           This is step 5, part 1:  M = msgHash mod gorder
        */
        mod_params.x = msgHash;
        mod_params.r = msgHash;

        printf ("[START] Modular reduction operation\n");
        ret = engine_lwpka_modular_reduction_locked(engine, &mod_params);
        printf ("[END] Modular reduction operation = 0x%x\n", ret);
        CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(ret);

        /*
           This is step 6:  k_ilwerse = 1/k mod gorder
        */
        mod_params.x = randNum;
        mod_params.r = randNumIlwerse;

        printf ("[START] Modular ilwersion operation\n");
        ret = engine_lwpka_modular_ilwersion_locked(engine, &mod_params);
        printf ("[END] Modular ilwersion operation = 0x%x\n", ret);
        CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(ret);

        /*
           sigS = d*sigR mod gorder
           This is step 4, part 2:   sigS = d (step 4.1) * R mod gorder (step 3)
        */
        mod_params.x = privateKey;
        mod_params.y = sigR;
        mod_params.r = sigS;

        printf ("[START] Modular multiplicaton operation\n");
        ret = engine_lwpka_modular_multiplication_locked(engine, &mod_params);
        printf ("[END] Modular multiplication operation = 0x%x\n", ret);
        CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(ret);

        /*
           sigS = M + sigS mod gorder
           This is step 5, part 2:  M (step 5.1) + sigS (step 4.2) mod gorder
        */
        mod_params.x = msgHash; 
        mod_params.y = sigS;
        mod_params.r = sigS;

        printf ("[START] Modular addition operation\n");
        ret = engine_lwpka_modular_addition_locked(engine, &mod_params);
        printf ("[END] Modular addition operation = 0x%x\n", ret);
        CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(ret);

        /*
           sigS = sigS*(1/k) mod gorder
           This is step 7: sigS (step 5.2) * k_ilwerse (step 6) mod gorder
        */
        mod_params.x = sigS; 
        mod_params.y = randNumIlwerse;
        mod_params.r = sigS;

        printf ("[START] Modular addition operation\n");
        ret = engine_lwpka_modular_multiplication_locked(engine, &mod_params);
        printf ("[END] Modular addition operation = 0x%x\n", ret);
        CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(ret);

        /*
           Check if sigS is zero, if it is start over
        */
        if (!memCompare(sigS, bigZero, algorithmByteSize))
        {
            //continue;
            printf("[ERROR] SigS should not be 0!\n");
            return;
        }

    /*
        //
        // If we reach this point, SUCCESS!
        // End the loop
        //
        break;
    }
    */

#ifdef TEST_MUTEX
    printf ("[START] Release mutex\n");
    lwpka_release_mutex(engine);
    printf ("[END] Release mutex\n");

    printf ("[START] Read mutex status\n");
#if defined LIBCCC_FSP_SE_ENABLED
    data = readWrapper((volatile void *)0x0151011c);
#else
    data = dioSeReadWrapper((volatile void *)0x0000011c);
#endif
    printf ("[END] Mutex status = 0x%x\n", data);
#endif

    printf("[START] ECDSA generated signature validation\n");
    if (memCompare(sigR, R, algorithmByteSize))
    {
        printf("[ERROR] SigR does not match expected value!\n");
        return;
    }
    if (memCompare(sigS, S, algorithmByteSize))
    {
        printf("[ERROR] SigS does not match expected value!\n");
        return;
    }
    printf("[END] ECDSA generated signature validation successful\n");
}

static void verifyEcdsaP256Signing(void)
{
#include "test-vector-ecdsa-p256-signing.h"
    verifyEcdsaSigningCommon(SIZE_IN_BYTES_ECDSA_P256, TE_LWRVE_NIST_P_256, (uint8_t*)&msgHash, (uint8_t*)&privateKey, (uint8_t*)&randNum, (uint8_t*)&R, (uint8_t*)&S);
}

static void verifyEcdsaP384Signing(void)
{
#include "test-vector-ecdsa-p384-signing.h"
    verifyEcdsaSigningCommon(SIZE_IN_BYTES_ECDSA_P384, TE_LWRVE_NIST_P_384, (uint8_t*)&msgHash, (uint8_t*)&privateKey, (uint8_t*)&randNum, (uint8_t*)&R, (uint8_t*)&S);
}

void verifyEcdsaSigning(void)
{
    printf("\n* P-256 lwrve *\n");
    verifyEcdsaP256Signing();

    printf("\n* P-384 lwrve *\n");
    verifyEcdsaP384Signing();
}
