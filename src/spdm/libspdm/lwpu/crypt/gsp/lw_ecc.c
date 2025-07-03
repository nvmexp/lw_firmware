/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

// Included libSPDM copyright, as file is LW-authored but uses libSPDM code.

/*!
 * @file   lw_ecc.c
 * @brief  File that provides ECC functionality for ECDHE and ECDSA via
 *         existing SEC2 implementations, for usage in libSPDM.
 */

/* ------------------------- System Includes -------------------------------- */
#include "base.h"
#include "library/cryptlib.h"
#include "lw_crypt.h"
#include "lw_utils.h"
#include "tegra_se.h"
#include "tegra_lwpka_mod.h"
#include "tegra_pka1_ec_gen.h"
#include "crypto_ec.h"
#include "tegra_lwpka_gen.h"
#include <liblwriscv/print.h>
#include <liblwriscv/libc.h>
#include <liblwriscv/sha.h>

/* ---------------------------  Defines ------------------------------------ */
#define RELEASE_MUTEX()                 lwpka_release_mutex(engine)
#define CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(retVal)  \
    if (retVal != NO_ERROR)                             \
    {                                                   \
        RELEASE_MUTEX();                                \
        return FALSE;                                   \
    }

//
// The constants for the lwrve NIST P-384 aka secp384r1
// The ECC domain parameters are suggested by SECG https://www.secg.org/
// Check SEC 2: Recommended Elliptic Lwrve Domain Parameteres, Version 2.0
//
static LwU32 ECC_P384_Order[ECC_P384_INTEGER_SIZE_IN_DWORDS]
    GCC_ATTRIB_SECTION("dmem_spdm", "ECC_P384_Order") =
{
    0xCCC52973, 0xECEC196A, 0x48B0A77A, 0x581A0DB2, 0xF4372DDF,
    0xC7634D81, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF 
};

//
// Context struct provided as option for ECC key management. Lifetime is from
// ec_new_by_nid to ec_free calls. Protected by g_ecContextMutex.
//
static EC_CONTEXT g_ecContext
  GCC_ATTRIB_SECTION("dmem_spdm", "g_ecContext");

status_t  GCC_ATTRIB_SECTION("imem_libspdm", "libcccECCGetKeyPair") 
libcccECCGetKeyPair(struct te_ec_point_s* publicKey, uint8_t* privateKey, uint32_t keySizeBytes);

boolean GCC_ATTRIB_SECTION("imem_libspdm", "libcccEccComputeKey")
libcccEccComputeKey(uint8 *peerPublicLocal, uintn peer_public_size, uint8 *privLocal, uint8 *key, uintn *key_size);

boolean GCC_ATTRIB_SECTION("imem_libspdm", "ec_generate_ecc_key_pair")
ec_generate_ecc_key_pair(OUT uint32 *pPubKey, OUT uint32 *pPrivKey);

static int GCC_ATTRIB_SECTION("imem_libspdm", "bigIntCompare")
bigIntCompare(const LwU32 *pX, const LwU32 *pY, const LwU32  digits);

static boolean GCC_ATTRIB_SECTION("imem_libspdm", "_libcccGen_ecc_priv_key")
_libcccGen_ecc_priv_key(uint32 *pPrivKey);

/* ------------------------- Public Functions ------------------------------- */
/**
  Allocates and Initializes one Elliptic Lwrve context for subsequent use
  with the NID.

  @param nid cipher NID

  @return  Pointer to the Elliptic Lwrve context that has been initialized.
           If the allocations fails, ec_new_by_nid() returns NULL.

**/
void *ec_new_by_nid(IN uintn nid)
{
    if (nid != CRYPTO_NID_SECP384R1)
    {
        return NULL;
    }
    // TODO: rbhenwal Need to add mutex acquire here

    // Reset context state. Considered initialized once keys are set.
    zero_mem(&g_ecContext, sizeof(g_ecContext));
    g_ecContext.bInitialized = LW_FALSE;

    return &g_ecContext;
}

/**
  Release the specified EC context.

  @param[in]  ec_context  Pointer to the EC context to be released.

**/
void ec_free(IN void *ec_context)
{
    // Nothing to do if incorrect context.
    if (ec_context != (void *)&g_ecContext)
    {
        return;
    }

    zero_mem(&g_ecContext, sizeof(g_ecContext));
    // TODO: rbhenwal Release mutex acquire in ec_new_by_nid
}

/*
 * @brief Generate ECC Key Pair
 *
 * @param publicKey:    public key is a struct ptr containing a point on the lwrve
 * @param privateKey:   private key (only known by private key owner) used to generate public key
 * @param privateKeySizeBytes: key size in bytes (only 384b is lwrrently supported)
 *
 * @return status_t:    NO_ERROR on success, error (from error_code.h) on failure
 */
status_t
libcccECCGetKeyPair(
    struct te_ec_point_s* publicKey,
    uint8_t*              privateKey,
    uint32_t              privateKeySizeBytes
)
{
    status_t                     ret             = NO_ERROR;
    engine_t                    *engine          = NULL;
    struct se_data_params        input_params    = {0};
    struct se_data_params        verifyPoint     = {0};
    struct se_engine_ec_context  ec_context      = {0};
    struct te_ec_point_s         generatorPoint  = {0};

    ret = ccc_select_engine((const struct engine_s **)&engine, ENGINE_CLASS_EC, ENGINE_PKA);
    if (ret != NO_ERROR)
    {
        return FALSE;
    }

    if(privateKeySizeBytes != ECC_P384_PRIVKEY_SIZE_IN_BYTES)
    {
        return FALSE;
    }

    ret = lwpka_acquire_mutex(engine);
    if (ret != NO_ERROR)
    {
        return FALSE;
    }

    ec_context.engine       = engine;
    ec_context.cmem         = NULL;
    ec_context.ec_flags     = CCC_EC_FLAGS_DYNAMIC_KEYSLOT;
    ec_context.ec_keytype   = KEY_TYPE_EC_PRIVATE;
    ec_context.algorithm    = TE_ALG_ECDH;
    ec_context.domain       = TE_CRYPTO_DOMAIN_UNKNOWN;
    ec_context.ec_pkey      = privateKey;
    ec_context.ec_pubkey    = publicKey;
    ec_context.ec_lwrve     = ccc_ec_get_lwrve(TE_LWRVE_NIST_P_384);

    memcpy(generatorPoint.x, ec_context.ec_lwrve->g.gen_x, privateKeySizeBytes);
    memcpy(generatorPoint.y, ec_context.ec_lwrve->g.gen_y, privateKeySizeBytes);

    generatorPoint.point_flags = CCC_EC_POINT_FLAG_NONE;

    ec_context.ec_k          = privateKey;
    ec_context.ec_k_length   = privateKeySizeBytes;
    input_params.point_P     = &generatorPoint;
    input_params.point_Q     = publicKey;
    input_params.input_size  = sizeof(te_ec_point_t);
    input_params.output_size = sizeof(te_ec_point_t);

    ret = engine_lwpka_ec_point_mult_locked(&input_params, &ec_context);
    CCC_ERROR_CHECK(ret);

    verifyPoint.point_P    = publicKey;
    verifyPoint.input_size = sizeof(te_ec_point_t);

    // Verify that Public key generated is on NIST_P384 lwrve
    ret = engine_lwpka_ec_point_verify_locked(&verifyPoint, &ec_context);
    CCC_ERROR_CHECK(ret);

fail:
    lwpka_release_mutex(engine);
    return ret;
}

/*!
 * @brief Compares two big integers for equality (X == Y).
 *
 * @param[in]  pX
 *      Pointer to the first value.
 *
 * @param[in]  pY
 *      Pointer to the second value.
 *
 * @param[in]  digits
 *      Number of digits to compare.
 *
 * @returns -1, 0, 1 based on whether X is less than, equal to, or greater than
 *      Y, respectively.
 */
static int
bigIntCompare
(
    const LwU32 *pX,
    const LwU32 *pY,
    const LwU32  digits
)
{
    int i;

    for (i = digits - 1; i >= 0; i--)
    {
        if (pX[i] > pY[i])
        {
            return 1;
        }
        else if (pX[i] < pY[i])
        {
            return -1;
        }
    }
    return 0;
}

/*!
  Generates a private key for NIST P-384 lwrve by generating random
  number modulo the order of the lwrve. Ensures private key is
  nonzero.

  @note Private key is output in little-endian format.

  @param pPrivKey  Pointer to buffer which will hold private key.
                   Must be of size ECC_P384_PRIVKEY_SIZE_IN_DWORDS.

  @return boolean  TRUE if successful, FALSE otherwise.
 */
static boolean
_libcccGen_ecc_priv_key
(
    uint32 *pPrivKey
)
{
    engine_t                     *engine         = NULL;
    engine_t                     *rngEngine      = NULL;
    struct se_engine_rng_context  rngContext     = {0};
    struct se_data_params         rngParams      = {0};
    status_t                      ret            = NO_ERROR;
    LwBool                        bStatusGood    = LW_FALSE;
    LwU32                         i              = 0;
    LwU32                         zero384[ECC_P384_PRIVKEY_SIZE_IN_DWORDS];

    if (pPrivKey == NULL)
    {
        return FALSE;
    }

    //
    // Initialize zeroed buffer the size of the private key.
    // Used for modulus operation, and to ensure resulting key is nonzero.
    //
    zero_mem(zero384, sizeof(zero384));

    if ((ccc_select_engine((const struct engine_s **)&engine, ENGINE_CLASS_EC, ENGINE_PKA) != NO_ERROR))
    {
        return FALSE;
    }

    if (lwpka_acquire_mutex(engine) != NO_ERROR)
    {
        return FALSE;
    }

    ret = ccc_select_engine((const struct engine_s **)&rngEngine, ENGINE_CLASS_AES, ENGINE_SE0_AES1); 
    if (ret != NO_ERROR)
    {
        bStatusGood = LW_FALSE;
        goto ErrorExit;
    }

    rngContext.engine      = rngEngine;
    rngParams.dst          = (uint8_t*)pPrivKey;
    rngParams.output_size  = ECC_P384_PRIVKEY_SIZE_IN_BYTES;

    // Keep looping until success, or we have reached max attempts.
    for (i = 0; i < ECC_GEN_PRIV_KEY_MAX_RETRIES; i++)
    {
        ret = engine_genrnd_generate_locked(&rngParams, &rngContext);
        if (ret != NO_ERROR)
        {
            bStatusGood = LW_FALSE;
            goto ErrorExit;
        }

        // Reject all values outside the range [1, order-1]
        if (bigIntCompare(pPrivKey, ECC_P384_Order,
                          ECC_P384_PRIVKEY_SIZE_IN_DWORDS) >= 0)
        {
            continue;
        }

        // Keep looping if key is zero
        if (bigIntCompare(pPrivKey, zero384,
                          ECC_P384_PRIVKEY_SIZE_IN_DWORDS) != 0)
        {
            bStatusGood = LW_TRUE;
            break;
        }
    }
ErrorExit:
    lwpka_release_mutex(engine); 
    return (bStatusGood == LW_TRUE && ret == NO_ERROR);
}

/*!
  Generates an EC key pair for the NIST P-384 lwrve.

  @note Outputs keys in big-endian format.

  @param pPubKey   Pointer to public key, must have size of
                   ECC_P384_PUBKEY_SIZE_IN_DWORDS.
  @param pPrivKey  Pointer to private key, must have size of
                   ECC_P384_PRIVKEY_SIZE_IN_DWORDS.

  @return TRUE if key pair is successfully generated,
          FALSE otherwise.
 */
boolean
ec_generate_ecc_key_pair
(
    OUT uint32 *pPubKey,
    OUT uint32 *pPrivKey
)
{
    struct te_ec_point_s keyPub384     = {0};
    status_t             ret           = NO_ERROR;

    if (pPubKey == NULL || pPrivKey == NULL)
    {
        return FALSE;
    }

    // Generate Private Key 
    if(!_libcccGen_ecc_priv_key(pPrivKey))
    {
        return FALSE;   
    }

    // Generate Public key using the previously generated private key
    ret = libcccECCGetKeyPair(&keyPub384, (LwU8*)pPrivKey, ECC_P384_PRIVKEY_SIZE_IN_BYTES);
    if (ret != NO_ERROR)
    {
        return FALSE;
    }

    copy_mem(pPubKey, keyPub384.x, ECC_P384_INTEGER_SIZE_IN_BYTES);
    copy_mem((pPubKey + ECC_P384_INTEGER_SIZE_IN_DWORDS), keyPub384.y, ECC_P384_INTEGER_SIZE_IN_BYTES);

    return TRUE;
}

/**
  Sets the public key component into the established EC context.

  For P-256, the public_size is 64. first 32-byte is X, second 32-byte is Y.
  For P-384, the public_size is 96. first 48-byte is X, second 48-byte is Y.
  For P-521, the public_size is 132. first 66-byte is X, second 66-byte is Y.

  @param[in, out]  ec_context      Pointer to EC context being set.
  @param[in]       public         Pointer to the buffer to receive generated public X,Y.
  @param[in]       public_size     The size of public buffer in bytes.

  @retval  TRUE   EC public key component was set successfully.
  @retval  FALSE  Invalid EC public key component.

**/
boolean ec_set_pub_key(IN OUT void *ec_context, IN uint8 *public_key,
               IN uintn public_key_size)
{
    //
    // TODO GSP-CC CONFCOMP-575: Implement ECC-384 functionality.
    // Determine if we can leave this function unimplemented,
    // as it is lwrrently unused.
    //
    return FALSE;
}

/**
  Gets the public key component from the established EC context.

  For P-256, the public_size is 64. first 32-byte is X, second 32-byte is Y.
  For P-384, the public_size is 96. first 48-byte is X, second 48-byte is Y.
  For P-521, the public_size is 132. first 66-byte is X, second 66-byte is Y.

  @param[in, out]  ec_context      Pointer to EC context being set.
  @param[out]      public         Pointer to the buffer to receive generated public X,Y.
  @param[in, out]  public_size     On input, the size of public buffer in bytes.
                                  On output, the size of data returned in public buffer in bytes.

  @retval  TRUE   EC key component was retrieved successfully.
  @retval  FALSE  Invalid EC key component.

**/
boolean ec_get_pub_key(IN OUT void *ec_context, OUT uint8 *public_key,
               IN OUT uintn *public_key_size)
{
    //
    // TODO GSP-CC CONFCOMP-575: Implement ECC-384 functionality.
    // Determine if we can leave this function unimplemented,
    // as it is lwrrently unused.
    //
    return FALSE;
}

/**
  Validates key components of EC context.
  NOTE: This function performs integrity checks on all the EC key material, so
        the EC key structure must contain all the private key data.

  If ec_context is NULL, then return FALSE;.

  @param[in]  ec_context  Pointer to EC context to check.

  @retval  TRUE   EC key components are valid.
  @retval  FALSE  EC key components are not valid.

**/
boolean ec_check_key(IN void *ec_context)
{
    //
    // TODO GSP-CC CONFCOMP-575: Implement ECC-384 functionality.
    // Determine if we can leave this function unimplemented,
    // as it is lwrrently unused.
    //
    return FALSE;
}

/**
  This function interacts with libCCC of lwriscv to compute the secret.
  Secret is X component of a random point on secp384r1 defined lwrve
  X is the first half of public with size being public_size / 2,

  If public_size is NULL, then return FALSE.
  If public_size is large enough but public is NULL, then return FALSE
  If key or key_size is NULL then return FALSE
  If key_size is less than ECC_P384_SECRET_SIZE_IN_BYTES then return FALSE
  If secret generated sucessfully returns TRUE
  @retVal key      : Secret generated
  @ retVal key_size: size of secret generated

**/
boolean
libcccEccComputeKey
(
    uint8 *peerPublicLocal,
    uintn  peer_public_size, 
    uint8 *privLocal,
    uint8 *key,
    uintn *key_size)
{
    status_t                     ret                                             = NO_ERROR;                                            
    engine_t                    *engine                                          = NULL;                                                
    struct se_engine_ec_context  lwpkaEcContext                                  = {0};
    struct se_data_params        input_params                                    = {0};
    struct se_data_params        verifyPoint                                     = {0}; 
    struct te_ec_point_s         generatorPoint                                  = {0};
    struct te_ec_point_s         sharedSecretPoint                               = {0};

    if (peerPublicLocal == NULL || privLocal == NULL || key == NULL || key_size == NULL)
    {
        return FALSE;
    }

    if (peer_public_size != ECC_P384_PUBKEY_SIZE_IN_BYTES ||
        *key_size < ECC_P384_SECRET_SIZE_IN_BYTES)
    {
        return FALSE;
    }

    ret = ccc_select_engine((const struct engine_s **)&engine, ENGINE_CLASS_EC, ENGINE_PKA);                   
    if (ret != NO_ERROR)
    {
        return FALSE;
    }

    ret = lwpka_acquire_mutex(engine);
    CCC_ERROR_CHECK(ret);                                                                                      

    memcpy(generatorPoint.x, peerPublicLocal, ECC_P384_INTEGER_SIZE_IN_BYTES);                                      
    memcpy(generatorPoint.y, (peerPublicLocal + ECC_P384_INTEGER_SIZE_IN_BYTES), ECC_P384_INTEGER_SIZE_IN_BYTES);
    generatorPoint.point_flags = CCC_EC_POINT_FLAG_NONE;

    lwpkaEcContext.engine      = engine;                                                                          
    lwpkaEcContext.cmem        = NULL;
    lwpkaEcContext.ec_flags    = CCC_EC_FLAGS_DYNAMIC_KEYSLOT;                                                    
    lwpkaEcContext.ec_keytype  = KEY_TYPE_EC_PRIVATE;                                                             
    lwpkaEcContext.algorithm   = TE_ALG_ECDH;                                                                     
    lwpkaEcContext.domain      = TE_CRYPTO_DOMAIN_UNKNOWN;                                                        
    lwpkaEcContext.ec_pkey     = (LwU8*)privLocal;
    lwpkaEcContext.ec_pubkey   = &sharedSecretPoint;
    lwpkaEcContext.ec_lwrve    = ccc_ec_get_lwrve(TE_LWRVE_NIST_P_384);
    lwpkaEcContext.ec_k        = (LwU8*)privLocal;
    lwpkaEcContext.ec_k_length = ECC_P384_PRIVKEY_SIZE_IN_BYTES;

    input_params.point_P     = &generatorPoint;
    input_params.point_Q     = &sharedSecretPoint;
    input_params.input_size  = sizeof(te_ec_point_t);   
    input_params.output_size = sizeof(te_ec_point_t);

    ret = engine_lwpka_ec_point_verify_locked(&input_params, &lwpkaEcContext);
    CCC_ERROR_CHECK(ret);

    ret = engine_lwpka_ec_point_mult_locked(&input_params, &lwpkaEcContext);
    CCC_ERROR_CHECK(ret);

    verifyPoint.point_P     = &sharedSecretPoint;
    verifyPoint.input_size  = sizeof(te_ec_point_t);   

    ret = engine_lwpka_ec_point_verify_locked(&verifyPoint, &lwpkaEcContext);
    CCC_ERROR_CHECK(ret);

    *key_size = ECC_P384_SECRET_SIZE_IN_BYTES;
    memcpy((LwU8*)key, (LwU8*)&(sharedSecretPoint.x), *key_size);

fail:
    lwpka_release_mutex(engine); 
    return (ret == NO_ERROR);
}

/**
  Generates EC key and returns EC public key (X, Y).

  This function generates random secret, and computes the public key (X, Y), which is
  returned via parameter public, public_size.
  X is the first half of public with size being public_size / 2,
  Y is the second half of public with size being public_size / 2.
  EC context is updated accordingly.
  If the public buffer is too small to hold the public X, Y, FALSE is returned and
  public_size is set to the required buffer size to obtain the public X, Y.

  For P-256, the public_size is 64. first 32-byte is X, second 32-byte is Y.
  For P-384, the public_size is 96. first 48-byte is X, second 48-byte is Y.
  For P-521, the public_size is 132. first 66-byte is X, second 66-byte is Y.

  If ec_context is NULL, then return FALSE;.
  If public_size is NULL, then return FALSE;.
  If public_size is large enough but public is NULL, then return FALSE;.

  @param[in, out]  ec_context      Pointer to the EC context.
  @param[out]      public         Pointer to the buffer to receive generated public X,Y.
  @param[in, out]  public_size     On input, the size of public buffer in bytes.
                                  On output, the size of data returned in public buffer in bytes.

  @retval TRUE   EC public X,Y generation succeeded.
  @retval FALSE  EC public X,Y generation failed.
  @retval FALSE  public_size is not large enough.

**/
boolean ec_generate_key(IN OUT void *ec_context, OUT uint8 *public_key,
            IN OUT uintn *public_key_size)
{
    EC_CONTEXT *pContext = (EC_CONTEXT *)ec_context;

    if (pContext == NULL || public_key == NULL || public_key_size == NULL ||
        *public_key_size < ECC_P384_PUBKEY_SIZE_IN_DWORDS)
    {
        return LW_FALSE;
    }

    if (!ec_generate_ecc_key_pair(pContext->ecPublic,
                                  pContext->ecPrivate))
    {
        return LW_FALSE;
    }

    pContext->bInitialized = LW_TRUE;
    *public_key_size       = ECC_P384_PUBKEY_SIZE_IN_BYTES;

    copy_mem(public_key, pContext->ecPublic, *public_key_size);
    return TRUE;
}

/**
  Computes exchanged common key.

  Given peer's public key (X, Y), this function computes the exchanged common key,
  based on its own context including value of lwrve parameter and random secret.
  X is the first half of peer_public with size being peer_public_size / 2,
  Y is the second half of peer_public with size being peer_public_size / 2.

  If ec_context is NULL, then return FALSE;.
  If peer_public is NULL, then return FALSE;.
  If peer_public_size is 0, then return FALSE;.
  If key is NULL, then return FALSE;.
  If key_size is not large enough, then return FALSE;.

  For P-256, the peer_public_size is 64. first 32-byte is X, second 32-byte is Y.
  For P-384, the peer_public_size is 96. first 48-byte is X, second 48-byte is Y.
  For P-521, the peer_public_size is 132. first 66-byte is X, second 66-byte is Y.

  @param[in, out]  ec_context          Pointer to the EC context.
  @param[in]       peer_public         Pointer to the peer's public X,Y.
  @param[in]       peer_public_size     size of peer's public X,Y in bytes.
  @param[out]      key                Pointer to the buffer to receive generated key.
  @param[in, out]  key_size            On input, the size of key buffer in bytes.
                                      On output, the size of data returned in key buffer in bytes.

  @retval TRUE   EC exchanged key generation succeeded.
  @retval FALSE  EC exchanged key generation failed.
  @retval FALSE  key_size is not large enough.

**/
boolean ec_compute_key(IN OUT void *ec_context, IN const uint8 *peer_public,
               IN uintn peer_public_size, OUT uint8 *key,
               IN OUT uintn *key_size)
{
    EC_CONTEXT  *pContext                                        = (EC_CONTEXT *)ec_context;
    LwU8         privLocal[ECC_P384_PRIVKEY_SIZE_IN_BYTES]       = {0};
    LwU8         peerPublicLocal[ECC_P384_PUBKEY_SIZE_IN_BYTES]  = {0};

    if (pContext == NULL || peer_public == NULL || key == NULL || key_size == NULL)
    {
        return FALSE;
    }

    if (peer_public_size != ECC_P384_PUBKEY_SIZE_IN_BYTES ||
        *key_size < ECC_P384_INTEGER_SIZE_IN_BYTES)
    {
        return FALSE;
    }

    zero_mem(privLocal, sizeof(privLocal));
    zero_mem(peerPublicLocal, sizeof(peerPublicLocal));

    copy_mem(peerPublicLocal, peer_public, peer_public_size);

    copy_mem((LwU8 *)privLocal, (LwU8 *)&(pContext->ecPrivate),
                           ECC_P384_INTEGER_SIZE_IN_BYTES);

    if (!libcccEccComputeKey(peerPublicLocal, peer_public_size, privLocal, key, key_size))
    {
        return FALSE;
    }

    return TRUE;
}

/**
  Carries out the EC-DSA signature using libCCC interface APIs

  @param[in]       local_hash   Pointer to octet message hash to be signed.
  @param[in]       localPrivate Pointer to private key used to sign the message hash
  @param[out]      localSig     Pointer to buffer to receive EC-DSA signature.
  @param[out]      sig_size     On output, the size of data returned in signature buffer in bytes.

  @retval  TRUE   signature successfully generated in EC-DSA.
  @retval  FALSE  signature generation failed.
**/
static boolean libcccECDSASignHash(LwU8 *localHash, LwU8* localPrivate, LwU8 *localSig, LwU32 *sig_size)
{
    engine_t                     *engine                                          = NULL;
    te_mod_params_t               modParams                                       = {0};
    struct te_ec_point_s          point_P                                         = {0};
    struct te_ec_point_s          point_Q                                         = {0};
    struct se_data_params         input                                           = {0};
    engine_t                     *rngEngine                                       = NULL;
    struct se_data_params         input1                                          = {0};
    struct se_engine_rng_context  rngContext                                      = {0};
    LwU8                          randNum[ECC_P384_INTEGER_SIZE_IN_BYTES]         = {0};
    struct se_engine_ec_context   lwpkaEcContext                                  = {0};
    LwU8                          bigZero[ECC_P384_INTEGER_SIZE_IN_BYTES]         = {0};
    LwU8                          randNumIlwerse[ECC_P384_INTEGER_SIZE_IN_BYTES]  = {0};
    LwU32                         numAttempts                                     = 0;
    const struct pka1_ec_lwrve_s *ec_lwrve                                        = ccc_ec_get_lwrve(TE_LWRVE_NIST_P_384);

    if (localHash == NULL || localPrivate == NULL || localSig == NULL || 
        sig_size == NULL  || ec_lwrve == NULL)
    {
        return FALSE;
    }

    if ((ccc_select_engine((const struct engine_s **)&engine, ENGINE_CLASS_EC, ENGINE_PKA) != NO_ERROR))
    {
        return FALSE;
    }

    if (lwpka_acquire_mutex(engine) != NO_ERROR)
    {
        return FALSE;
    }

    input1.dst         = randNum;
    input1.output_size = ECC_P384_INTEGER_SIZE_IN_BYTES;

    for (numAttempts=0; numAttempts < ECC_SIGN_MAX_RETRIES; numAttempts++)
    {
        CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(ccc_select_engine((const struct engine_s **)&rngEngine, ENGINE_CLASS_AES, ENGINE_SE0_AES1)); 
        rngContext.engine  = rngEngine;


        CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(engine_genrnd_generate_locked(&input1, &rngContext));

        // Continue if randNum is all zeros
        if (!memcmp(randNum, bigZero, ECC_P384_INTEGER_SIZE_IN_BYTES))
        {
            continue;
        }

        CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(ccc_select_engine((const struct engine_s **)&engine, ENGINE_CLASS_EC, ENGINE_PKA));

        // Initialize params which are not going to change in the loop
        modParams.size     = ECC_P384_INTEGER_SIZE_IN_BYTES;
        modParams.op_mode  = PKA1_OP_MODE_ECC384;

        memcpy(point_P.x, ec_lwrve->g.gen_x, ECC_P384_INTEGER_SIZE_IN_BYTES);
        memcpy(point_P.y, ec_lwrve->g.gen_y, ECC_P384_INTEGER_SIZE_IN_BYTES);

        point_P.point_flags = CCC_EC_POINT_FLAG_NONE;

        input.point_P     = &point_P;
        input.input_size  = sizeof(point_P);
        input.point_Q     = &point_Q;
        input.output_size = sizeof(point_Q);

        lwpkaEcContext.engine       = engine;
        lwpkaEcContext.ec_flags     = CCC_EC_FLAGS_DYNAMIC_KEYSLOT;
        lwpkaEcContext.ec_keytype   = KEY_TYPE_EC_PRIVATE;
        lwpkaEcContext.ec_lwrve     = ec_lwrve;

        /*
         * localSig = X point of point multiplication operation
         * This is step 2, part 2:  point multiplication of randNum * P(GenX, GenY), R = X1
         */
        memset(&point_Q, 0, sizeof(struct te_ec_point_s));
        lwpkaEcContext.ec_k         = randNum;
        lwpkaEcContext.ec_k_length  = ECC_P384_INTEGER_SIZE_IN_BYTES;

        CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(engine_lwpka_ec_point_mult_locked(&input, &lwpkaEcContext));

        /*
         * localSig = localSig mod gorder
         * This is step 3:  R = X1 (from step 2.2) mod gOrder
         */
        modParams.x = point_Q.x;
        modParams.m = lwpkaEcContext.ec_lwrve->n;
        modParams.r = localSig;

        CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(engine_lwpka_modular_reduction_locked(engine, &modParams));

        // Start signature computation from begining if localSig is all zeros
        if (!memcmp(localSig, bigZero, ECC_P384_POINT_SIZE_IN_BYTES))
        {
            continue;
        }

        /*
         * This is step 4, part 1:  d = privateKey mod gorder
         */ 
        modParams.x = localPrivate;
        modParams.r = localPrivate;

        CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(engine_lwpka_modular_reduction_locked(engine, &modParams));

        /*
         * This is step 5, part 1:  M = msgHash mod gorder
         */
        modParams.x = localHash;
        modParams.r = localHash;

        CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(engine_lwpka_modular_reduction_locked(engine, &modParams));

        /*
         * This is step 6:  k_ilwerse = 1/k mod gorder
         */
        modParams.x = randNum;
        modParams.r = randNumIlwerse;

        CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(engine_lwpka_modular_ilwersion_locked(engine, &modParams));

        /*
         * localSig[ECC_P384_INTEGER_SIZE_IN_DWORDS] = d*localSig mod gorder
         *            This is step 4, part 2:   localSig[ECC_P384_INTEGER_SIZE_IN_DWORDS] = d (step 4.1) * R mod gorder (step 3)
         */
        modParams.x = localPrivate;
        modParams.y = localSig;
        modParams.r = &localSig[ECC_P384_INTEGER_SIZE_IN_BYTES];

        CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(engine_lwpka_modular_multiplication_locked(engine, &modParams));

        /*
         * localSig[ECC_P384_INTEGER_SIZE_IN_DWORDS] = M + localSig[ECC_P384_INTEGER_SIZE_IN_DWORDS] mod gorder
         *            This is step 5, part 2:  M (step 5.1) + localSig[ECC_P384_INTEGER_SIZE_IN_DWORDS] (step 4.2) mod gorder
         */
        modParams.x = localHash;
        modParams.y = &localSig[ECC_P384_INTEGER_SIZE_IN_BYTES];
        modParams.r = &localSig[ECC_P384_INTEGER_SIZE_IN_BYTES];

        CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(engine_lwpka_modular_addition_locked(engine, &modParams));

        /*
         *  localSig[ECC_P384_INTEGER_SIZE_IN_DWORDS] = localSig[ECC_P384_INTEGER_SIZE_IN_DWORDS]*(1/k) mod gorder
         *             This is step 7: localSig[ECC_P384_INTEGER_SIZE_IN_DWORDS] (step 5.2) * k_ilwerse (step 6) mod gorder
         */
        modParams.x = &localSig[ECC_P384_INTEGER_SIZE_IN_BYTES];
        modParams.y = randNumIlwerse;
        modParams.r = &localSig[ECC_P384_INTEGER_SIZE_IN_BYTES];

        CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(engine_lwpka_modular_multiplication_locked(engine, &modParams));

        // Start signature computation from begining if localSig is all zeros
        if (!memcmp(&localSig[ECC_P384_INTEGER_SIZE_IN_DWORDS], bigZero, ECC_P384_POINT_SIZE_IN_BYTES))
        {
            continue;
        }

        *sig_size = ECC_P384_POINT_SIZE_IN_BYTES;
        break;
    }

    lwpka_release_mutex(engine); 

    if (numAttempts == ECC_SIGN_MAX_RETRIES)
    {
        return FALSE;
    }

    return TRUE;
}

/**
  Carries out the EC-DSA signature.

  This function carries out the EC-DSA signature.
  If the signature buffer is too small to hold the contents of signature, FALSE
  is returned and sig_size is set to the required buffer size to obtain the signature.

  If ec_context is NULL, then return FALSE;.
  If message_hash is NULL, then return FALSE;.
  If hash_size need match the hash_nid. hash_nid could be SHA256, SHA384, SHA512, SHA3_256, SHA3_384, SHA3_512.
  If sig_size is large enough but signature is NULL, then return FALSE;.

  For P-256, the sig_size is 64. first 32-byte is R, second 32-byte is S.
  For P-384, the sig_size is 96. first 48-byte is R, second 48-byte is S.
  For P-521, the sig_size is 132. first 66-byte is R, second 66-byte is S.

  @param[in]       ec_context    Pointer to EC context for signature generation.
  @param[in]       hash_nid      hash NID
  @param[in]       message_hash  Pointer to octet message hash to be signed.
  @param[in]       hash_size     size of the message hash in bytes.
  @param[out]      signature    Pointer to buffer to receive EC-DSA signature.
  @param[in, out]  sig_size      On input, the size of signature buffer in bytes.
                                On output, the size of data returned in signature buffer in bytes.

  @retval  TRUE   signature successfully generated in EC-DSA.
  @retval  FALSE  signature generation failed.
  @retval  FALSE  sig_size is too small.

**/
boolean ecdsa_sign(IN void *ec_context, IN uintn hash_nid,
           IN const uint8 *message_hash, IN uintn hash_size,
           OUT uint8 *signature, IN OUT uintn *sig_size)
{
    EC_CONTEXT    *pContext                                    = (EC_CONTEXT *)ec_context;
    LwU8          localPrivate[ECC_P384_PRIVKEY_SIZE_IN_BYTES];
    LwU8          localHash[SHA_384_HASH_SIZE_BYTE];
    LwU8          localSig[ECC_P384_POINT_SIZE_IN_BYTES];

    if (ec_context == NULL || message_hash == NULL || signature == NULL || sig_size == NULL)
    {
        return FALSE;
    }
    // PNIST_SHA-384 is only supported
    if (hash_nid != CRYPTO_NID_SHA384 || hash_size != SHA384_DIGEST_SIZE)
    {
        return FALSE;
    }
    if (*sig_size < (ECC_P384_POINT_SIZE_IN_BYTES))
    {
        return FALSE;
    }

    zero_mem(localHash,    sizeof(localHash));
    zero_mem(localSig,     sizeof(localSig));
    zero_mem(localPrivate, sizeof(localPrivate)); 

    copy_mem(localHash, message_hash, SHA384_DIGEST_SIZE);
    
    copy_mem(localPrivate, (const LwU8*)&pContext->ecPrivate,
                           ECC_P384_PRIVKEY_SIZE_IN_BYTES);

    if (!libcccECDSASignHash(localHash, localPrivate, localSig, (LwU32*)sig_size))
    {
        return FALSE;
    }

    copy_mem(signature, localSig, ECC_P384_INTEGER_SIZE_IN_BYTES);
    copy_mem(&signature[ECC_P384_INTEGER_SIZE_IN_BYTES], &(localSig[ECC_P384_INTEGER_SIZE_IN_BYTES]), ECC_P384_INTEGER_SIZE_IN_BYTES);
    
    return TRUE;
}

/**
  Verifies the EC-DSA signature.

  If ec_context is NULL, then return FALSE;.
  If message_hash is NULL, then return FALSE;.
  If signature is NULL, then return FALSE;.
  If hash_size need match the hash_nid. hash_nid could be SHA256, SHA384, SHA512, SHA3_256, SHA3_384, SHA3_512.

  For P-256, the sig_size is 64. first 32-byte is R, second 32-byte is S.
  For P-384, the sig_size is 96. first 48-byte is R, second 48-byte is S.
  For P-521, the sig_size is 132. first 66-byte is R, second 66-byte is S.

  @param[in]  ec_context    Pointer to EC context for signature verification.
  @param[in]  hash_nid      hash NID
  @param[in]  message_hash  Pointer to octet message hash to be checked.
  @param[in]  hash_size     size of the message hash in bytes.
  @param[in]  signature    Pointer to EC-DSA signature to be verified.
  @param[in]  sig_size      size of signature in bytes.

  @retval  TRUE   Valid signature encoded in EC-DSA.
  @retval  FALSE  Invalid signature or invalid EC context.
**/
boolean ecdsa_verify(IN void *ec_context, IN uintn hash_nid,
             IN const uint8 *message_hash, IN uintn hash_size,
             IN const uint8 *signature, IN uintn sig_size)
{
    // TODO: GSP-CC CONFCOMP-575 Implement ECDSA-384 functionality.
    return TRUE;
}
