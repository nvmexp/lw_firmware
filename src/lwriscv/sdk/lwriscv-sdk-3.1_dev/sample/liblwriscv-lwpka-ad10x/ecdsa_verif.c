/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <liblwriscv/libc.h>

#include "tegra_se.h"
#include "tegra_lwrng.h"
#include "tegra_lwpka_gen.h"
#include "tegra_lwpka_ec.h"
#include "tegra_lwpka_mod.h"
#include "crypto_ec.h"
#include "ecdsa_verif.h"
#include "ecc_key_gen.h"

#include "utils.h"
#include "ecdsa.h"
#include "debug.h"

/*!
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 * This code is for testing and libCCC usage purposes only!
 * Do not replicate blindly for production usecases. Consult with your local crypto expert!
 * 
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 * */

#define ECDSA_KEY_BYTES_MAX 48

/** @brief Verify ECDSA Signature given a public Signature and Key Pair
 *
 * @param keySizeBytes: key size in bytes (only 256b and 384b are lwrrently supported)
 * @param lwrveId: ECC lwrve to use
 * @param msgHash: hash of plaintext message
 * @param keyPubPoint: public key point from which we'll callwlate R
 * @param R: Public (expected) R to verify against
 * @param S: Public Signature
 *
 * @note: Workflow derived from https://secg.org/sec1-v2.pdf section 4.1.4
 *
 * @return status_t: NO_ERROR on success, error (from error_code.h) on failure
 */
static status_t verifyEcdsaSignature(
    uint32_t keySizeBytes,
    te_ec_lwrve_id_t lwrveId,
    uint8_t *msgHash,
    struct te_ec_point_s* keyPubPoint,
    uint8_t *R,
    uint8_t *S
);

/** @brief Sign ECDSA message
 *
 * @param keySizeBytes: key size in bytes (only 256b and 384b are lwrrently supported)
 * @param lwrveId: ECC lwrve to use for signing
 * @param msgHash: hash of plaintext message
 * @param d: private key for signature generation
 * @param k: private key for key-pair generation
 * @param keyPubPoint: Generated public key point
 * @param S: Generated public S (part of signature)
 * @param R: Generated public R (part of signature)
 *
 * @return status_t: NO_ERROR on success, error (from error_code.h) on failure
 */
static status_t ecdsaSign(
    uint32_t keySizeBytes,
    te_ec_lwrve_id_t lwrveId,
    uint8_t *msgHash,
    uint8_t *d,
    uint8_t *k,
    struct te_ec_point_s* keyPubPoint,
    uint8_t *S,
    uint8_t *R
);

static void ecdsaVerifyLwrve256(void);
static void ecdsaVerifyLwrve384(void);

#define VERIFY(inputPoint) do {                                             \
    input_params.point_P = (inputPoint);                                    \
    input_params.point_Q = NULL;                                            \
    input_params.input_size = sizeof(te_ec_point_t);                        \
    input_params.output_size = 0;                                           \
    ret = engine_lwpka_ec_point_verify_locked(&input_params, &ec_context);  \
    CCC_ERROR_CHECK(ret);                                                   \
} while(0)

#define MULTIPLYPOINT(k, p, q) do {                                         \
    ec_context.ec_k = (k);                                                  \
    ec_context.ec_k_length  = keySizeBytes;                                 \
    input_params.point_P = (p);                                             \
    input_params.point_Q = (q);                                             \
    input_params.input_size = sizeof(te_ec_point_t);                        \
    input_params.output_size = sizeof(te_ec_point_t);                       \
    ret = engine_lwpka_ec_point_mult_locked(&input_params, &ec_context);    \
    CCC_ERROR_CHECK(ret);                                                   \
} while(0)

#define MULTIPLYMODULAR(inputA, inputB, output) do {                        \
    mod_params.x = inputA;                                                  \
    mod_params.y = inputB;                                                  \
    mod_params.r = output;                                                  \
    mod_params.m = ec_context.ec_lwrve->n;                                  \
    mod_params.size = keySizeBytes;                                         \
    ret = engine_lwpka_modular_multiplication_locked(engine, &mod_params);  \
    CCC_ERROR_CHECK(ret);                                                   \
} while(0)

#define REDUCE(input, output) do {                                          \
    mod_params.x = input;                                                   \
    mod_params.y = NULL;                                                    \
    mod_params.r = output;                                                  \
    mod_params.m = ec_context.ec_lwrve->n;                                  \
    mod_params.size = keySizeBytes;                                         \
    ret = engine_lwpka_modular_reduction_locked(engine, &mod_params);       \
    CCC_ERROR_CHECK(ret);                                                   \
} while(0)

#define ILWERT(input, output) do {                                          \
    mod_params.m = ec_context.ec_lwrve->n;                                  \
    mod_params.x = input;                                                   \
    mod_params.y = NULL;                                                    \
    mod_params.r = output;                                                  \
    mod_params.size = keySizeBytes;                                         \
    ret = engine_lwpka_modular_ilwersion_locked(engine, &mod_params);       \
    CCC_ERROR_CHECK(ret);                                                   \
} while(0)

#define ADDMODULAR(inputA, inputB, output) do {                             \
    mod_params.x = inputA;                                                  \
    mod_params.y = inputB;                                                  \
    mod_params.r = output;                                                  \
    mod_params.m = ec_context.ec_lwrve->n;                                  \
    mod_params.size = keySizeBytes;                                         \
    ret = engine_lwpka_modular_addition_locked(engine, &mod_params);        \
    CCC_ERROR_CHECK(ret);                                                   \
} while(0)

/* EC_K (EC_K_LENGTH) and EC_L (EC_L_LENGTH) must be set by caller
 *
 * Q = (k x P) + (l x Q)
 */
#define SHAMIRTRICK(inputK, generatorPoint, inputL, pubKeyPoint) do {       \
    ec_context.ec_k = inputK;                                               \
    ec_context.ec_k_length = keySizeBytes;                                  \
    ec_context.ec_l = inputL;                                               \
    ec_context.ec_l_length = keySizeBytes;                                  \
    input_params.point_P = (generatorPoint);                                \
    input_params.input_size = sizeof(te_ec_point_t);                        \
    input_params.point_Q = (pubKeyPoint);                                   \
    input_params.output_size = sizeof(te_ec_point_t);                       \
    ret = engine_lwpka_ec_shamir_trick_locked(&input_params, &ec_context);  \
    CCC_ERROR_CHECK(ret);                                                   \
} while(0)


#define ECDSA_PAIR_SIGN_VERIFY(lwrve, keySizeBytes) do {                                        \
    struct te_ec_point_s keyPub = {0};                                                          \
    uint8_t RCalc[ECDSA_KEY_BYTES_MAX] = {0};                                                   \
    uint8_t SCalc[ECDSA_KEY_BYTES_MAX] = {0};                                                   \
    status_t ret = eccGetKeyPair(TE_ALG_ECDSA, &keyPub, privateKey, keySizeBytes);              \
    CCC_ERROR_CHECK(ret);                                                                       \
    ret= ecdsaSign(keySizeBytes, lwrve, msgHash, privateKey, randNum, &keyPub, SCalc, RCalc);   \
    CCC_ERROR_CHECK(ret);                                                                       \
    if(!((memcmp(SCalc, S, sizeof(S)) == 0) &&                                                  \
       (memcmp(RCalc, R, sizeof(R)) == 0))) {                                                   \
        printf("Signature did not match expected\n");                                           \
        DUMP_MEM("S_expect", S, keySizeBytes);                                                  \
        DUMP_MEM("S_actual", SCalc, keySizeBytes);                                              \
        DUMP_MEM("R_expect", R, keySizeBytes);                                                  \
        DUMP_MEM("R_actual", RCalc, keySizeBytes);                                              \
    }                                                                                           \
    ret = verifyEcdsaSignature(keySizeBytes, lwrve, msgHash, &keyPub, RCalc, SCalc);            \
    CCC_ERROR_CHECK(ret);                                                                       \
} while(0)


/** @brief Verify 256b and 384b ECDSA Signature
 *
 * @param None
 *
 * @return None
 */
void ecdsaVerify(void)
{
    ecdsaVerifyLwrve256();
    ecdsaVerifyLwrve384();
}

/** @brief Verify ECDSA Signature given a public Signature and Key Pair
 *
 * @param keySizeBytes: key size in bytes (only 256b and 384b are lwrrently supported)
 * @param lwrveId: ECC lwrve to use
 * @param msgHash: hash of plaintext message
 * @param keyPubPoint: public key point from which we'll callwlate R
 * @param R: Public (expected) R to verify against
 * @param S: Public Signature
 *
 * @note: Workflow derived from https://secg.org/sec1-v2.pdf section 4.1.4
 *
 * @return status_t: NO_ERROR on success, error (from error_code.h) on failure
 */
static status_t verifyEcdsaSignature
(
    uint32_t keySizeBytes,
    __attribute__((unused)) te_ec_lwrve_id_t lwrveId,
    uint8_t *msgHash,
    struct te_ec_point_s* keyPubPoint,
    uint8_t *R,
    uint8_t *S
)
{
    struct se_engine_ec_context  ec_context                          = {0};
    struct se_data_params        input_params                        = {0};
    te_mod_params_t              mod_params                          = {0};
    engine_t                     *engine                             = NULL;
    status_t                     ret                                 = NO_ERROR;

    uint8_t bigZero[ECDSA_KEY_BYTES_MAX]        = {0};
    uint8_t SIlwerse[ECDSA_KEY_BYTES_MAX]       = {0};
    uint8_t msgHashReduced[ECDSA_KEY_BYTES_MAX] = {0};
    uint8_t u1[ECDSA_KEY_BYTES_MAX] = {0};
    uint8_t u2[ECDSA_KEY_BYTES_MAX] = {0};

    struct te_ec_point_s                generatorPoint  = {0};
    struct te_ec_point_s                RCalc           = {0};
    memcpy((uint8_t*) &RCalc, (uint8_t*) keyPubPoint, sizeof(RCalc));

    if((memcmp(R, bigZero, keySizeBytes) == 0) ||
       (memcmp(S, bigZero, keySizeBytes) == 0))
    {
        ret = ERR_ILWALID_ARGS;
        return ret;
    }

    ret = ccc_select_engine((const struct engine_s **)&engine, ENGINE_CLASS_EC, ENGINE_PKA);
    CCC_ERROR_CHECK(ret);

    ec_context.engine       = engine;
    ec_context.cmem         = NULL;
    ec_context.ec_flags     = CCC_EC_FLAGS_DYNAMIC_KEYSLOT;
    ec_context.ec_keytype   = KEY_TYPE_EC_PUBLIC;
    ec_context.ec_pubkey    = keyPubPoint;
    ec_context.ec_k_length  = keySizeBytes;
    ec_context.algorithm    = TE_ALG_ECDSA;
    te_ec_lwrve_id_t lwrve  = TE_LWRVE_NONE;
    enum pka1_op_mode op    = PKA1_OP_MODE_ILWALID;

    switch((keySizeBytes << 3))
    {
        case 256:
            lwrve = TE_LWRVE_NIST_P_256;
            op = PKA1_OP_MODE_ECC256;
            break;
        case 384:
            lwrve = TE_LWRVE_NIST_P_384;
            op = PKA1_OP_MODE_ECC384;
            break;
        default:
            break;
    }

    ec_context.ec_lwrve = ccc_ec_get_lwrve(lwrve); // const struct pka1_ec_lwrve_t
    mod_params.op_mode  = op;    
    mod_params.size     = keySizeBytes;

    ACQUIRE_MUTEX();
    CCC_ERROR_CHECK(ret);

    // Get Generator Point
    memcpy(generatorPoint.x, ec_context.ec_lwrve->g.gen_x, keySizeBytes);
    memcpy(generatorPoint.y, ec_context.ec_lwrve->g.gen_y, keySizeBytes);

    /*
        High-level workflow is as follows (from https://secg.org/sec1-v2.pdf section 4.1.4):
            u1 = modReduce(mgsHash * pow(S,-1))
            u2 = modReduce(R * pow(S,-1))
            Ra = u1 * G
            Rb = u2 * Qu
            RCallwnreduced = Ra + Rb
            RCalc = modReduce(RCallwnreduced)
            if(RCalc == R)
                pass
            else
                fail
    */

    // Ilwert Signature
    ILWERT(S, SIlwerse);

    // Reduce Msg Hash
    REDUCE(msgHash, msgHashReduced);

    // LHS (u1 = s^(-1) * msgHashReduced)
    MULTIPLYMODULAR(msgHashReduced, SIlwerse, u1);

    // RHS (u2 = s^(-1) * R)
    MULTIPLYMODULAR(R, SIlwerse, u2);

    // SHAMIR TRICK (RCalc = u1*Generator + u2*PublicKey) last parameter is input (as public key) and output (as shamir trick)
    SHAMIRTRICK(u1, &generatorPoint, u2, &RCalc);

fail:
    RELEASE_MUTEX();

    DUMP_MEM("Input R ", R, keySizeBytes);
    DUMP_MEM("Output R", RCalc.x, keySizeBytes);

    TRACE_NN("ECDSA %d SIGNATURE VERIFICATION = ", (keySizeBytes << 3));
    if((ret == NO_ERROR) && (memcmp(R, RCalc.x, keySizeBytes) == 0))
    {
        TRACE("success");
    }
    else
    {
        TRACE("failed; err = %d\n", ret);
    }
    return ret;
}

/** @brief Sign ECDSA message
 *
 * @param keySizeBytes: key size in bytes (only 256b and 384b are lwrrently supported)
 * @param lwrveId: ECC lwrve to use for signing
 * @param msgHash: hash of plaintext message
 * @param d: private key for signature generation
 * @param k: private key for key-pair generation
 * @param keyPubPoint: Generated public key point
 * @param S: Generated public S (part of signature)
 * @param R: Generated public R (part of signature)
 *
 * @return status_t: NO_ERROR on success, error (from error_code.h) on failure
 */
static status_t ecdsaSign(
    uint32_t keySizeBytes,
    __attribute__((unused)) te_ec_lwrve_id_t lwrveId,
    uint8_t *msgHash,
    uint8_t *d,
    uint8_t *k,
    __attribute__((unused))  struct te_ec_point_s* keyPubPoint,
    uint8_t *S,
    uint8_t *R
)
{
    struct se_engine_ec_context  ec_context                          = {0};
    te_mod_params_t              mod_params                          = {0};
    engine_t                     *engine                             = NULL;
    status_t                     ret                                 = 0;

    uint8_t kIlwerse[ECDSA_KEY_BYTES_MAX] = {0};
    uint8_t u1[ECDSA_KEY_BYTES_MAX] = {0};
    uint8_t u2[ECDSA_KEY_BYTES_MAX] = {0};

    struct te_ec_point_s                signingPoint  = {0};

    ret = eccGetKeyPair(TE_ALG_ECDSA, &signingPoint, k, keySizeBytes);
    CCC_ERROR_CHECK(ret);

    ret = ccc_select_engine((const struct engine_s **)&engine, ENGINE_CLASS_EC, ENGINE_PKA);
    CCC_ERROR_CHECK(ret);

    ACQUIRE_MUTEX();    
    CCC_ERROR_CHECK(ret);

    ec_context.engine       = engine;
    ec_context.cmem         = NULL;
    ec_context.ec_flags     = CCC_EC_FLAGS_DYNAMIC_KEYSLOT;
    
    ec_context.ec_keytype   = KEY_TYPE_EC_PRIVATE;
    
    ec_context.ec_k         = k;
    ec_context.ec_k_length  = keySizeBytes;

    ec_context.algorithm    = TE_ALG_ECDSA;
    ec_context.domain       = TE_CRYPTO_DOMAIN_UNKNOWN;

    te_ec_lwrve_id_t lwrve  = TE_LWRVE_NONE;
    enum pka1_op_mode op = PKA1_OP_MODE_ILWALID;
    switch((keySizeBytes << 3))
    {
        case 256:
            lwrve = TE_LWRVE_NIST_P_256;
            op = PKA1_OP_MODE_ECC256;
            break;
        case 384:
            lwrve = TE_LWRVE_NIST_P_384;
            op = PKA1_OP_MODE_ECC384;
            break;
        default:
            break;
    }

    ec_context.ec_lwrve = ccc_ec_get_lwrve(lwrve); // const struct pka1_ec_lwrve_t
    mod_params.op_mode = op;    
    mod_params.size = keySizeBytes;

    REDUCE(signingPoint.x, R);
    ILWERT(k, kIlwerse);
    MULTIPLYMODULAR(R, d, u1);
    ADDMODULAR(msgHash, u1, u2);
    MULTIPLYMODULAR(kIlwerse, u2, S);

fail:
    RELEASE_MUTEX();
    printf("ECDSA %d SIGNATURE GENERATION = ", (keySizeBytes << 3));
    if(ret == NO_ERROR)
    {
        printf("success\n");
    }
    else
    {
        printf("failed; err = %d\n", ret);
    }
    return ret;
}

static void ecdsaVerifyLwrve256(void)
{
    #include "test-vector-ecdsa-p256-signing.h"
    ECDSA_PAIR_SIGN_VERIFY(TE_LWRVE_NIST_P_256, 32);
fail:
    return;
};

static void ecdsaVerifyLwrve384(void)
{
    #include "test-vector-ecdsa-p384-signing.h"
    ECDSA_PAIR_SIGN_VERIFY(TE_LWRVE_NIST_P_384, 48);
fail:
    return;
};
