/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
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

// Included libspdm copyright, as file is LW-authored but uses libspdm code.

/*!
 * @file   lw_ecc.c
 * @brief  File that provides ECC functionality for ECDHE and ECDSA via
 *         existing SEC2 implementations, for usage in libspdm.
 */

/* ------------------------- System Includes -------------------------------- */
#include "base.h"
#include "lwuproc.h"
#include "lwrtos.h"
#include "lwos_ovl_dmem.h"
#include "lw_utils.h"

/* ------------------------- Application Includes --------------------------- */
#include "library/cryptlib.h"
#include "lw_sec2_rtos.h"
#include "lw_crypt.h"
#include "seapi.h"
#include "bigint.h"
#include "lib_shahw.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define EC_MUTEX_100US_TIMEOUT_IN_TICKS (LWRTOS_TIME_US_TO_TICKS(100))

/* ------------------------- Prototypes ------------------------------------- */
static boolean _ec_gen_ecc_priv_key(OUT uint32 *pPrivKey)
    GCC_ATTRIB_SECTION("imem_libspdm", "_ec_gen_ecc_priv_key");

/* ------------------------ Static Variables ------------------------------- */
/* The constants for the lwrve NIST P-256 aka secp256r1 aka secp256v1 */
LwU32 ECC_P256_A[ECC_P256_INTEGER_SIZE_IN_DWORDS]
    GCC_ATTRIB_SECTION("dmem_spdm", "ECC_P256_A") =
{
    0xfffffffc, 0xffffffff, 0xffffffff, 0x00000000,
    0x00000000, 0x00000000, 0x00000001, 0xffffffff,
};

LwU32 ECC_P256_B[ECC_P256_INTEGER_SIZE_IN_DWORDS]
    GCC_ATTRIB_SECTION("dmem_spdm", "ECC_P256_B") =
{
    0x27d2604b, 0x3bce3c3e, 0xcc53b0f6, 0x651d06b0,
    0x769886bc, 0xb3ebbd55, 0xaa3a93e7, 0x5ac635d8,
};

LwU32 ECC_P256_GenPoint[ECC_P256_POINT_SIZE_IN_DWORDS]
    GCC_ATTRIB_SECTION("dmem_spdm", "ECC_P256_GenPoint") =
{
    0xd898c296, 0xf4a13945, 0x2deb33a0, 0x77037d81,
    0x63a440f2, 0xf8bce6e5, 0xe12c4247, 0x6b17d1f2,
    0x37bf51f5, 0xcbb64068, 0x6b315ece, 0x2bce3357,
    0x7c0f9e16, 0x8ee7eb4a, 0xfe1a7f9b, 0x4fe342e2,
};

LwU32 ECC_P256_Modulus[ECC_P256_INTEGER_SIZE_IN_DWORDS]
    GCC_ATTRIB_SECTION("dmem_spdm", "ECC_P256_Modulus") =
{
   0xffffffff, 0xffffffff, 0xffffffff, 0x00000000,
   0x00000000, 0x00000000, 0x00000001, 0xffffffff
};

LwU32 ECC_P256_Order[ECC_P256_INTEGER_SIZE_IN_DWORDS]
    GCC_ATTRIB_SECTION("dmem_spdm", "ECC_P256_Order") =
{
    0xfc632551, 0xf3b9cac2, 0xa7179e84, 0xbce6faad,
    0xffffffff, 0xffffffff, 0x00000000, 0xffffffff
};

//
// Context struct provided as option for ECC key management. Lifetime is from
// ec_new_by_nid to ec_free calls. Protected by g_ecContextMutex.
//
static EC_CONTEXT g_ecContext
  GCC_ATTRIB_SECTION("dmem_spdm", "g_ecContext");

//
// Mutex to synchronize usage of ec_context. Must be initialized 
// by call to spdm_ec_context_mutex_initialize().
// APM-TODO CONFCOMP-400: Replace with HS compatible mutex.
//
LwrtosSemaphoreHandle g_ecContextMutex
    GCC_ATTRIB_SECTION("dmem_spdm", "g_ecContextMutex");

/* ------------------------- Static Functions ------------------------------- */
/*!
  Generates a private key for NIST P-256 lwrve by generating random
  number modulo the order of the lwrve. Ensures private key is
  nonzero.

  @note Private key is output in little-endian format.
  
  @param pPrivKey  Pointer to buffer which will hold private key.
                   Must be of size ECC_P256_PRIVKEY_SIZE_IN_DWORDS.
  
  @return boolean  TRUE if successful, FALSE otherwise.
 */
static boolean
_ec_gen_ecc_priv_key
(
    OUT uint32 *pPrivKey
)
{
    LwBool        bStatusGood = LW_FALSE;
    LwU32         i           = 0;
    LwU32         zero256[ECC_P256_PRIVKEY_SIZE_IN_DWORDS];

    //
    // Initialize zeroed buffer the size of the private key.
    // Used to ensure resulting key is nonzero.
    //
    zero_mem(zero256, sizeof(zero256));


    // Keep looping until success, or we have reached max attempts.
    for (i = 0; i < ECC_GEN_PRIV_KEY_MAX_RETRIES; i++)
    {
        if (seTrueRandomGetNumber(
                pPrivKey, ECC_P256_PRIVKEY_SIZE_IN_DWORDS) != SE_OK)
        {
            bStatusGood = LW_FALSE;
            goto ErrorExit;
        }

        // Reject all values outside the range [1, order-1]
        if (bigIntCompare(pPrivKey, ECC_P256_Order,
                          ECC_P256_PRIVKEY_SIZE_IN_DWORDS) >= 0)
        {
            continue;
        }

        // Keep looping if key is zero
        if (bigIntCompare(pPrivKey, zero256,
                          ECC_P256_PRIVKEY_SIZE_IN_DWORDS) != 0)
        {
            bStatusGood = LW_TRUE;
            break;
        }
    }

ErrorExit:

    return bStatusGood;
}

/* ------------------------- Lw_crypt.h Functions --------------------------- */
/**
  Initialize semaphore used to protect ec_context for ECC implementations.

  @note Must be called before any EC operations are used, the behavior
  is undefined otherwise.

  @return boolean  TRUE if successful, FALSE otherwise.
**/
boolean
spdm_ec_context_mutex_initialize(void)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    CHECK_FLCN_STATUS(lwrtosSemaphoreCreateBinaryGiven(&g_ecContextMutex,
                                                       OVL_INDEX_OS_HEAP));

ErrorExit:

    return FLCN_STATUS_TO_BOOLEAN(flcnStatus);
}

/*!
  Generates an EC key pair for the NIST P-256 lwrve.

  @note Outputs keys in big-endian format.

  @param pPubKey   Pointer to public key, must have size of
                   ECC_P256_PUBKEY_SIZE_IN_DWORDS.
  @param pPrivKey  Pointer to private key, must have size of
                   ECC_P256_PRIVKEY_SIZE_IN_DWORDS.

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
    LwU32   *pPubPointX  = NULL;
    LwU32   *pPubPointY  = NULL;
    LwU32   *pGenPointX  = NULL;
    LwU32   *pGenPointY  = NULL;
    LwBool   bStatusGood = LW_FALSE;

    if (pPubKey == NULL || pPrivKey == NULL)
    {
        bStatusGood = LW_FALSE;
        goto ErrorExit;
    }

    // Generate random private key
    bStatusGood = _ec_gen_ecc_priv_key(pPrivKey);
    if (!bStatusGood)
    {
        goto ErrorExit;
    }

    // Obtain public key by multiplying generator point with private key
    pGenPointX = ECC_P256_GenPoint,
    pGenPointY = &(ECC_P256_GenPoint[ECC_P256_INTEGER_SIZE_IN_DWORDS]);
    pPubPointX = pPubKey;
    pPubPointY = &(pPubKey[ECC_P256_INTEGER_SIZE_IN_DWORDS]);

    if (seECPointMult(ECC_P256_Modulus, ECC_P256_A, pGenPointX, pGenPointY,
                      pPrivKey, pPubPointX, pPubPointY,
                      SE_ECC_KEY_SIZE_256) != SE_OK)
    {
        // Either SE engine failure, or private key otherwise invalid
        bStatusGood = LW_FALSE;
        goto ErrorExit;
    }

    // Ensure public key is on lwrve
    if (seECPointVerification(ECC_P256_Modulus, ECC_P256_A, ECC_P256_B,
                              pPubPointX, pPubPointY,
                              SE_ECC_KEY_SIZE_256) != SE_OK)
    {
        bStatusGood = LW_FALSE;
        goto ErrorExit;
    }

    // Swap keys to big-endian format.
    if (!change_endianness((LwU8 *)pPrivKey, (LwU8 *)pPrivKey,
                           ECC_P256_PRIVKEY_SIZE_IN_BYTES))
    {
        return LW_FALSE;
    }

    if (!change_endianness((LwU8 *)pPubPointX, (LwU8 *)pPubPointX,
                           ECC_P256_INTEGER_SIZE_IN_BYTES))
    {
        return LW_FALSE;
    }
    if (!change_endianness((LwU8 *)pPubPointY, (LwU8 *)pPubPointY,
                           ECC_P256_INTEGER_SIZE_IN_BYTES))
    {
        return LW_FALSE;
    }

ErrorExit:

    return bStatusGood;
}

/* ------------------------- Cryptlib.h Functions --------------------------- */
/**
  Initializes one Elliptic Lwrve context for subsequent use with the NID.
  Obtains g_ecContextMutex. Must be freed by later call to ec_free.

  @param nid  cipher NID

  @return  Pointer to the Elliptic Lwrve context that has been initialized.
           If the mutex acquisition fails, ec_new_by_nid() returns NULL.

**/
void *
ec_new_by_nid
(
    IN uintn nid
)
{
    if (nid != CRYPTO_NID_SECP256R1)
    {
        return NULL;
    }

    if (lwrtosSemaphoreTake(g_ecContextMutex,
                            EC_MUTEX_100US_TIMEOUT_IN_TICKS) != FLCN_OK)
    {
        return NULL;
    }

    // Reset context state. Considered initialized once keys are set.
    zero_mem(&g_ecContext, sizeof(g_ecContext));
    g_ecContext.bInitialized = LW_FALSE;

    return &g_ecContext;
}

/**
  Release the specified EC context, scrubbing the data and releasing the mutex.
  Must own g_ecContextMutex via prior call to ec_new_by_nid before calling.

  @param[in] ec_context  Pointer to the EC context to be released.

**/
void
ec_free
(
    IN void *ec_context
)
{
    // Nothing to do if incorrect context.
    if (ec_context != (void *)&g_ecContext)
    {
        return;
    }

    zero_mem(&g_ecContext, sizeof(g_ecContext));
    
    // Best effort to release mutex, as we can't really recover on failure.
    (void)lwrtosSemaphoreGive(g_ecContextMutex);

    return;
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
  If public_size is large enough but public is NULL, then return FALSE;

  @note Public key is output in big-endian format.

  @param[in, out] ec_context   Pointer to the EC context.
  @param[out]     public       Pointer to the buffer to receive generated public X,Y.
  @param[in, out] public_size  On input, the size of public buffer in bytes.
                               On output, the size of data returned in public buffer in bytes.

  @retval TRUE   EC public X,Y generation succeeded.
  @retval FALSE  EC public X,Y generation failed.
  @retval FALSE  public_size is not large enough.

**/
boolean
ec_generate_key
(
    IN OUT void  *ec_context,
    OUT    uint8 *public_key,
    IN OUT uintn *public_key_size
)
{
    EC_CONTEXT *pContext = (EC_CONTEXT *)ec_context;

    if (pContext == NULL || public_key == NULL || public_key_size == NULL ||
        *public_key_size < ECC_P256_PUBKEY_SIZE_IN_BYTES)
    {
        return LW_FALSE;
    }

    if (!ec_generate_ecc_key_pair(pContext->ecPublic,
                                  pContext->ecPrivate))
    {
        return LW_FALSE;
    }

    pContext->bInitialized = LW_TRUE;
    *public_key_size       = ECC_P256_PUBKEY_SIZE_IN_BYTES;
    copy_mem(public_key, pContext->ecPublic, *public_key_size);
    
    return LW_TRUE;
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

  @note Keys are expected in big-endian format, and secret is output in big-endian format.

  @param[in, out] ec_context        Pointer to the EC context.
  @param[in]      peer_public       Pointer to the peer's public X,Y.
  @param[in]      peer_public_size  Size of peer's public X,Y in bytes.
  @param[out]     key               Pointer to the buffer to receive generated key.
  @param[in, out] key_size          On input, the size of key buffer in bytes.
                                    On output, the size of data returned in key buffer in bytes.

  @retval TRUE   EC exchanged key generation succeeded.
  @retval FALSE  EC exchanged key generation failed.
  @retval FALSE  key_size is not large enough.

**/
boolean
ec_compute_key
(
    IN OUT void        *ec_context,
    IN     const uint8 *peer_public,
    IN     uintn        peer_public_size,
    OUT    uint8       *key,
    IN OUT uintn       *key_size
)
{
    LwU32      *secretX     = NULL;
    LwU32      *secretY     = NULL;
    LwU32      *peerPublicX = NULL;
    LwU32      *peerPublicY = NULL;
    EC_CONTEXT *pContext    = (EC_CONTEXT *)ec_context;

    // APM-TODO CONFCOMP-400: May need to rethink local key buffer for HS.
    LwU32       secretLocal[ECC_P256_SECRET_SIZE_IN_DWORDS];
    LwU32       privLocal[ECC_P256_PRIVKEY_SIZE_IN_DWORDS];
    LwU32       peerPublicLocal[ECC_P256_PUBKEY_SIZE_IN_DWORDS];

    if (pContext == NULL || peer_public == NULL || key == NULL || key_size == NULL)
    {
        return LW_FALSE;
    }

    if (peer_public_size != ECC_P256_PUBKEY_SIZE_IN_BYTES ||
        *key_size < ECC_P256_SECRET_SIZE_IN_BYTES)
    {
        return LW_FALSE;
    }

    // Work with local buffers to ensure proper alignment.
    zero_mem(secretLocal, sizeof(secretLocal));
    zero_mem(privLocal, sizeof(privLocal));
    zero_mem(peerPublicLocal, sizeof(peerPublicLocal));

    copy_mem(privLocal, (LwU8 *)&pContext->ecPrivate, sizeof(privLocal));
    copy_mem(peerPublicLocal, peer_public, peer_public_size);

    secretX     = secretLocal;
    secretY     = &(secretLocal[ECC_P256_INTEGER_SIZE_IN_DWORDS]);
    peerPublicX = peerPublicLocal;
    peerPublicY = &(peerPublicLocal[ECC_P256_INTEGER_SIZE_IN_DWORDS]);

    // Colwert keys to little-endian format.
    if (!change_endianness((LwU8 *)privLocal, (LwU8 *)privLocal,
                           ECC_P256_PRIVKEY_SIZE_IN_BYTES))
    {
        return LW_FALSE;
    }
    if (!change_endianness((LwU8 *)peerPublicX, (LwU8 *)peerPublicX,
                           ECC_P256_INTEGER_SIZE_IN_BYTES))
    {
        return LW_FALSE;
    }
    if (!change_endianness((LwU8 *)peerPublicY, (LwU8 *)peerPublicY,
                           ECC_P256_INTEGER_SIZE_IN_BYTES))
    {
        return LW_FALSE;
    }

    // Ensure peer public key is valid.
    if (seECPointVerification(ECC_P256_Modulus, ECC_P256_A, ECC_P256_B,
                              peerPublicX, peerPublicY,
                              SE_ECC_KEY_SIZE_256) != SE_OK)
    {
        return LW_FALSE;
    }
    // Callwlate shared secret.
    if (seECPointMult(ECC_P256_Modulus, ECC_P256_A,
                      peerPublicX, peerPublicY, privLocal,
                      secretX, secretY, SE_ECC_KEY_SIZE_256) != SE_OK)
    {
        return LW_FALSE;
    }
    // Ensure shared secret is valid.
    if (seECPointVerification(ECC_P256_Modulus, ECC_P256_A, ECC_P256_B,
                              secretX, secretY, SE_ECC_KEY_SIZE_256) != SE_OK)
    {
        return LW_FALSE;
    }
    //
    // Copy out shared secret to output, storing as big-endian format,
    // as this is used directly in key-schedule derivation.
    //
    *key_size = ECC_P256_INTEGER_SIZE_IN_BYTES;
    if (!change_endianness((LwU8 *)&secretLocal, (LwU8 *)&secretLocal,
                           *key_size))
    {
        return LW_FALSE;
    }
    copy_mem(key, secretLocal, *key_size);

    return LW_TRUE;
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

  @note Keys are expected in big-endian format, and signature is output in big-endian.

  @param[in]      ec_context    Pointer to EC context for signature generation.
  @param[in]      hash_nid      Hash NID
  @param[in]      message_hash  Pointer to octet message hash to be signed.
  @param[in]      hash_size     Size of the message hash in bytes.
  @param[out]     signature     Pointer to buffer to receive EC-DSA signature.
  @param[in, out] sig_size      On input, the size of signature buffer in bytes.
                                On output, the size of data returned in signature buffer in bytes.

  @retval TRUE   signature successfully generated in EC-DSA.
  @retval FALSE  signature generation failed.
  @retval FALSE  sig_size is too small.

**/
boolean
ecdsa_sign
(
    IN     void        *ec_context,
    IN     uintn        hash_nid,
    IN     const uint8 *message_hash,
    IN     uintn        hash_size,
    OUT    uint8       *signature,
    IN OUT uintn       *sig_size
)
{
    EC_CONTEXT *pContext = (EC_CONTEXT *)ec_context;
    LwU32       localHash[SHA_256_HASH_SIZE_DWORD];
    LwU32       localSig[ECC_P256_POINT_SIZE_IN_DWORDS];


    // APM-TODO CONFCOMP-400: May need to rethink local key buffer for HS.
    LwU32       localPrivate[ECC_P256_PRIVKEY_SIZE_IN_DWORDS];

    if (pContext == NULL || message_hash == NULL || signature == NULL ||
        sig_size == NULL)
    {
        return LW_FALSE;
    }

    // We support only SHA256
    if (hash_nid != CRYPTO_NID_SHA256 || hash_size != SHA256_DIGEST_SIZE)
    {
        return LW_FALSE;
    }
    if (*sig_size < (ECC_P256_POINT_SIZE_IN_BYTES))
    {
        return LW_FALSE;
    }

    zero_mem(localHash,    sizeof(localHash));
    zero_mem(localSig,     sizeof(localSig));
    zero_mem(localPrivate, sizeof(localPrivate));

    // Swap endianness of input data and private key, storing in local buffers.
    if (!change_endianness((uint8 *)&localHash, message_hash,
                           SHA256_DIGEST_SIZE))
    {
        return LW_FALSE;
    }

    if (!change_endianness((LwU8 *)&localPrivate, (LwU8 *)&pContext->ecPrivate,
                           ECC_P256_PRIVKEY_SIZE_IN_BYTES))
    {
        return LW_FALSE;
    }

    if (seECDSASignHash(ECC_P256_Modulus,
                        ECC_P256_Order,
                        ECC_P256_A,
                        ECC_P256_B,
                        ECC_P256_GenPoint,
                        &ECC_P256_GenPoint[ECC_P256_INTEGER_SIZE_IN_DWORDS],
                        localHash,
                        localPrivate,
                        ECC_SIGN_MAX_RETRIES,
                        localSig,
                        &localSig[ECC_P256_INTEGER_SIZE_IN_DWORDS],
                        SE_ECC_KEY_SIZE_256) != SE_OK)
    {
        return LW_FALSE;
    }

    // Colwert R to big-endian
    change_endianness(signature, (uint8 *)localSig,
                      ECC_P256_INTEGER_SIZE_IN_BYTES);

    // Colwert S to big-endian
    change_endianness(&signature[ECC_P256_INTEGER_SIZE_IN_BYTES],
                      (uint8 *)&(localSig[ECC_P256_INTEGER_SIZE_IN_DWORDS]),
                      ECC_P256_INTEGER_SIZE_IN_BYTES);

    *sig_size = ECC_P256_POINT_SIZE_IN_BYTES;

    return LW_TRUE;
}
