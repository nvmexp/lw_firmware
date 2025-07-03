#include <liblwriscv/libc.h>
#include <liblwriscv/print.h>

#include "tegra_se.h"
#include "tegra_lwpka_gen.h"
#include "tegra_lwpka_ec.h"
#include "tegra_lwpka_mod.h"
#include "crypto_ec.h"
#include "sha256.h"
#include "utils.h"
#include "ecdsa.h"


int verifyEcdsaP256Signing(void)
{
    /* Not used lwrrently */
    //uint32_t    numAttempts    = 0;
    //uint32_t    retryLoopCount = 1000;

// TODO: For some reason, defining this at the top initializes all variables to 0.
// Probably something to do with global variable initialization issues in sample app.
#include "test-vector-ecdsa.h"

    uint8_t                      sigR[SIZE_IN_BYTES]             = {0};
    uint8_t                      sigS[SIZE_IN_BYTES]             = {0};
    uint8_t                      bigZero[SIZE_IN_BYTES]          = {0};
    uint8_t                      randNumIlwerse[SIZE_IN_BYTES]   = {0};
    struct se_data_params        input                           = {0};
    struct se_engine_ec_context  ec_context                      = {0};
    te_mod_params_t              mod_params                      = {0};
    engine_t                     *engine                         = NULL;
    status_t                     ret                             = 0;
    const struct pka1_ec_lwrve_s *ec_lwrve                       = tegra_ec_get_lwrve(TE_LWRVE_NIST_P_256);
    struct te_ec_point_s         point_P                         = {0};
    struct te_ec_point_s         point_Q                         = {0};

    // TODO: Why do we need this? Need to confirm with Juki.
    struct pka1_engine_data lwpka_data = { .op_mode = PKA1_OP_MODE_ILWALID };

    printf ("[START] CheetAh select engine\n");
    ret = tegra_select_engine((const struct engine_s **)&engine, ENGINE_CLASS_EC, ENGINE_PKA);
    printf ("[END] CheetAh select engine = 0x%x\n", ret);

#ifdef TEST_MUTEX
    printf ("[START] Acquire mutex\n");
    ret = lwpka_acquire_mutex(engine);
    printf ("[END] Acquire mutex = 0x%x\n", ret);
#endif

    uint32_t data = 0;
    printf ("[START] Read mutex status\n");
#if defined SAMPLE_PROFILE_fsp
    data = readWrapper((volatile void *)0x0151011c);
#else
    data = dioSeReadWrapper((volatile void *)0x0000011c);
#endif
    printf ("[END] Mutex status = 0x%x\n", data);

    /* Initialize params which are not going to change in the loop */
    mod_params.size     = SIZE_IN_BYTES;
    mod_params.op_mode  = PKA1_OP_MODE_ECC256;

    memcpy(point_P.x, ec_lwrve->g.gen_x, SIZE_IN_BYTES);
    memcpy(point_P.y, ec_lwrve->g.gen_y, SIZE_IN_BYTES);
    point_P.point_flags = CCC_EC_POINT_FLAG_NONE;

    input.point_P     = &point_P;
    input.input_size  = sizeof(point_P);
    input.point_Q     = &point_Q;
    input.output_size = sizeof(point_Q);

    ec_context.engine       = engine;
    ec_context.ec_flags     = CCC_EC_FLAGS_DYNAMIC_KEYSLOT;
    ec_context.ec_keytype   = KEY_TYPE_EC_PRIVATE;
    ec_context.ec_lwrve     = ec_lwrve;
    ec_context.pka1_data    = &lwpka_data;

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
        if (!memCompare(randNum, bigZero, SIZE_IN_BYTES))
        {
            //continue;
            printf("[ERROR] Provided random number should not be 0!\n");
            return 1;
        }

        /*
           sigR = X point of point multiplication operation
           This is step 2, part 2:  point multiplication of randNum * P(GenX, GenY), R = X1
        */
        memset(&point_Q, 0, sizeof(struct te_ec_point_s));
        ec_context.ec_k         = (uint8_t *)randNum;
        ec_context.ec_k_length  = sizeof(randNum);

        printf ("[START] EC point multiplication operation\n");
        ret = engine_lwpka_ec_point_mult_locked(&input, &ec_context);
        printf ("[END] EC point multiplication operation = 0x%x\n", ret);

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

        /*
           Check if sigR is zero, if it is start over
        */
        if (!memCompare(sigR, bigZero, SIZE_IN_BYTES))
        {
            //continue;
            printf("[ERROR] SigR should not be 0!\n");
            return 1;
        }

        /*
           This is step 4, part 1:  d = privateKey mod gorder
        */
        mod_params.x = privateKey;
        mod_params.r = privateKey;

        printf ("[START] Modular reduction operation\n");
        ret = engine_lwpka_modular_reduction_locked(engine, &mod_params);
        printf ("[END] Modular reduction operation = 0x%x\n", ret);

        /*
           This is step 5, part 1:  M = msgHash mod gorder
        */
        mod_params.x = msgHash;
        mod_params.r = msgHash;

        printf ("[START] Modular reduction operation\n");
        ret = engine_lwpka_modular_reduction_locked(engine, &mod_params);
        printf ("[END] Modular reduction operation = 0x%x\n", ret);

        /*
           This is step 6:  k_ilwerse = 1/k mod gorder
        */
        mod_params.x = randNum;
        mod_params.r = randNumIlwerse;

        printf ("[START] Modular ilwersion operation\n");
        ret = engine_lwpka_modular_ilwersion_locked(engine, &mod_params);
        printf ("[END] Modular ilwersion operation = 0x%x\n", ret);

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

        /*
           Check if sigS is zero, if it is start over
        */
        if (!memCompare(sigS, bigZero, SIZE_IN_BYTES))
        {
            //continue;
            printf("[ERROR] SigS should not be 0!\n");
            return 1;
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
#endif

    printf("[START] ECDSA generated signature validation\n");
    if (memCompare(sigR, R, SIZE_IN_BYTES))
    {
        printf("[ERROR] SigR does not match expected value!\n");
        return 1;
    }
    if (memCompare(sigS, S, SIZE_IN_BYTES))
    {
        printf("[ERROR] SigS does not match expected value!\n");
        return 1;
    }
    printf("[END] ECDSA generated signature validation successful\n");
    return 0;
}
