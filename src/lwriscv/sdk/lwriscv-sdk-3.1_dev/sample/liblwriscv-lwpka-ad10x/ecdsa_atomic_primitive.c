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
#include <liblwriscv/print.h>

#include "tegra_se.h"
#include "tegra_lwpka_gen.h"
#include "tegra_lwpka_ec.h"
#include "tegra_lwpka_mod.h"
#include "crypto_ec.h"
#include "lwpka_ec_ks_ops.h"

#include "utils.h"
#include "ecdsa_atomic_primitive.h"

/*!
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 * This code is for testing and libCCC usage purposes only!
 * Do not replicate blindly for production usecases. Consult with your local crypto expert!
 * 
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 * */

#ifdef TEST_KEY_INSERTION
static void insertPrivateKeyInKeystore
(
    engine_t *engine,
    uint8_t *privateKey,
    te_ec_lwrve_id_t lwrveId
)
{
    status_t                     ret        = 0;
    struct se_engine_ec_context  ec_context = {0};

    ec_context.engine        = engine;
    ec_context.ec_lwrve      = ccc_ec_get_lwrve(lwrveId);
    ec_context.algorithm     = TE_ALG_KEYOP_INS;
    ec_context.ec_keytype    = KEY_TYPE_EC_PRIVATE;
    ec_context.ec_flags      = CCC_EC_FLAGS_KEYSTORE;
    ec_context.ec_pkey       = privateKey;

#if LWRISCV_IS_ENGINE_pmu
    ec_context.ec_ks.eks_mf.emf_user = CCC_MANIFEST_EC_KS_USER_PWR;
#elif LWRISCV_IS_ENGINE_lwdec
    ec_context.ec_ks.eks_mf.emf_user = CCC_MANIFEST_EC_KS_USER_LWDEC;
#elif LWRISCV_IS_ENGINE_sec
    ec_context.ec_ks.eks_mf.emf_user = CCC_MANIFEST_EC_KS_USER_SEC;
#elif LWRISCV_IS_ENGINE_gsp
    ec_context.ec_ks.eks_mf.emf_user = CCC_MANIFEST_EC_KS_USER_GSP;
#elif LWRISCV_IS_ENGINE_fsp
    ec_context.ec_ks.eks_mf.emf_user = CCC_MANIFEST_EC_KS_USER_FSP;
#else
    #error "Invalid Peregrine engine selected!"
#endif

    if (lwrveId == TE_LWRVE_NIST_P_256) {
        ec_context.pka_data.op_mode     = PKA1_OP_MODE_ECC256;
        ec_context.ec_ks.eks_mf.emf_sw  = 0xEC0;    /* 12 bit field */
    } else {
        ec_context.pka_data.op_mode     = PKA1_OP_MODE_ECC384;
        ec_context.ec_ks.eks_mf.emf_sw  = 0xEC1;    /* 12 bit field */
    }
    ec_context.ec_ks.eks_mf.emf_ex   = 0x0;      /* 01 bit field: false */
    ec_context.ec_ks.eks_keystore    = 0x0;      /* Only one keystore */
   
    printf ("[START] Insert private key\n");
    ret = engine_lwpka_ec_ks_keyop_locked(NULL, &ec_context);
    printf ("[END] Insert private key = 0x%x\n", ret);
    CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(ret);
}
#endif

static void verifyEcdsaSigningCommonAtomicPrimitive
(
    engine_t *engine,
    uint32_t algorithmByteSize,
    te_ec_lwrve_id_t lwrveId,
    char    *message,
    uint8_t *msgHash
#ifdef TEST_KEY_INSERTION
    ,
    uint8_t *Qx,
    uint8_t *Qy
#endif
)
{
    uint32_t                     i          = 0;
    status_t                     ret        = 0;
    struct se_data_params        input      = {0};
    struct se_engine_ec_context  ec_context = {0};
    struct te_ec_sig_s           destSig    = {0};

    ec_context.engine           = engine;
    ec_context.ec_lwrve         = ccc_ec_get_lwrve(lwrveId);
    ec_context.algorithm        = TE_ALG_ECDSA;
    ec_context.ec_keytype       = KEY_TYPE_EC_PRIVATE;
    ec_context.ec_flags         = CCC_EC_FLAGS_KEYSTORE;

#if LWRISCV_IS_ENGINE_pmu
    ec_context.ec_ks.eks_mf.emf_user = CCC_MANIFEST_EC_KS_USER_PWR;
#elif LWRISCV_IS_ENGINE_lwdec
    ec_context.ec_ks.eks_mf.emf_user = CCC_MANIFEST_EC_KS_USER_LWDEC;
#elif LWRISCV_IS_ENGINE_sec
    ec_context.ec_ks.eks_mf.emf_user = CCC_MANIFEST_EC_KS_USER_SEC;
#elif LWRISCV_IS_ENGINE_gsp
    ec_context.ec_ks.eks_mf.emf_user = CCC_MANIFEST_EC_KS_USER_GSP;
#elif LWRISCV_IS_ENGINE_fsp
    ec_context.ec_ks.eks_mf.emf_user = CCC_MANIFEST_EC_KS_USER_FSP;
#else
    #error "Invalid Peregrine engine selected!"
#endif

    if (lwrveId == TE_LWRVE_NIST_P_256) {
        ec_context.pka_data.op_mode     = PKA1_OP_MODE_ECC256;
        ec_context.ec_ks.eks_mf.emf_sw  = 0xEC0;    /* 12 bit field */
    } else {
        ec_context.pka_data.op_mode     = PKA1_OP_MODE_ECC384;
        ec_context.ec_ks.eks_mf.emf_sw  = 0xEC1;    /* 12 bit field */
    }
    ec_context.ec_ks.eks_mf.emf_ex   = 0x0;      /* 01 bit field: false */
    ec_context.ec_ks.eks_keystore    = 0x0;      /* Only one keystore */
   
    input.input_size        = algorithmByteSize;
    input.src_digest        = msgHash;
    input.output_size       = sizeof(destSig);
    input.dst_ec_signature  = &destSig;
 
    printf ("[START] ECDSA atomic signing primitive\n");
    ret = engine_lwpka_ec_ks_ecdsa_sign_locked(&input, &ec_context);
    printf ("[END] ECDSA atomic signing primitive = 0x%x\n", ret);
    CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(ret);

    /* TODO: Automated ECDSA verification using LWPKA when code is available */
    printf("ECDSA generated signatures:\n");
    printf("Verify using: $P4_ROOT/sw/lwriscv/main/sample/liblwriscv-lwpka-gh100/scripts/verifyEcdsa.py -m ");
    i=0;
    do {
        printf("%c", message[i]);
        i++;
    } while (message[i] != '\0');
    printf(" -x ");
#ifdef TEST_KEY_INSERTION
    for (i=0; i<algorithmByteSize; i++) {
        printf("%02x", Qx[i]);
    }
#else
    printf("<public_key_x>");
#endif
    printf(" -y ");
#ifdef TEST_KEY_INSERTION
    for (i=0; i<algorithmByteSize; i++) {
        printf("%02x", Qy[i]);
    }
#else
    printf("<public_key_y>");
#endif
    printf(" -r ");
    for (i=0; i<algorithmByteSize; i++) {
        printf("%02x", destSig.r[i]);
    }
    printf(" -s ");
    for (i=0; i<algorithmByteSize; i++) {
        printf("%02x", destSig.s[i]);
    }
    printf(" -c ");
    if (lwrveId == TE_LWRVE_NIST_P_256) {
        printf("NIST256p");
    } else {
        printf("NIST384p");
    }
    printf("\n");
}

static void verifyEcdsaP256SigningAtomicPrimitive(void)
{
#include "test-vector-ecdsa-p256-signing-atomic-primitive.h"
    uint32_t          data   = 0;
    status_t          ret    = 0;
    engine_t         *engine = NULL;
    te_ec_lwrve_id_t lwrveId = TE_LWRVE_NIST_P_256;

    printf ("[START] Select engine\n");
    ret = ccc_select_engine((const struct engine_s **)&engine, ENGINE_CLASS_EC, ENGINE_PKA);
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

#ifdef TEST_KEY_INSERTION
    insertPrivateKeyInKeystore(engine, privateKey, lwrveId);
#endif
    verifyEcdsaSigningCommonAtomicPrimitive(engine, SIZE_IN_BYTES_ECDSA_P256, lwrveId, message, msgHash
#ifdef TEST_KEY_INSERTION
    , Qx, Qy
#endif
    );

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
}

static void verifyEcdsaP384SigningAtomicPrimitive(void)
{
#include "test-vector-ecdsa-p384-signing-atomic-primitive.h"
    uint32_t          data   = 0;
    status_t          ret    = 0;
    engine_t         *engine = NULL;
    te_ec_lwrve_id_t lwrveId = TE_LWRVE_NIST_P_384;

    printf ("[START] Select engine\n");
    ret = ccc_select_engine((const struct engine_s **)&engine, ENGINE_CLASS_EC, ENGINE_PKA);
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

#ifdef TEST_KEY_INSERTION
    insertPrivateKeyInKeystore(engine, privateKey, lwrveId);
#endif
    verifyEcdsaSigningCommonAtomicPrimitive(engine, SIZE_IN_BYTES_ECDSA_P384, lwrveId, message, msgHash
#ifdef TEST_KEY_INSERTION
    , Qx, Qy
#endif
    );

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
}

void verifyEcdsaSigningAtomicPrimitive(void)
{
    printf("\n* P-256 lwrve *\n");
    verifyEcdsaP256SigningAtomicPrimitive();

    printf("\n* P-384 lwrve *\n");
    verifyEcdsaP384SigningAtomicPrimitive();
}
