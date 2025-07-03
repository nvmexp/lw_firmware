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

// Included libSPDM copyright, as file is LW-authored but uses libSPDM code.

/*!
 * @file   lw_crypt.h
 * @brief  Function declarations for custom crypto operations needed by LWPU.
 *         This file is meant to augment hal/library/cryptlib.h.
 */

#ifndef _LW_CRYPT_H_
#define _LW_CRYPT_H_
#include <lwtypes.h>

/* ------------------------ Macros and Defines ----------------------------- */
#define ECC_P384_INTEGER_SIZE_IN_DWORDS    (12)
#define ECC_P384_INTEGER_SIZE_IN_BYTES     (4 * ECC_P384_INTEGER_SIZE_IN_DWORDS)
#define ECC_P384_POINT_SIZE_IN_DWORDS      (2 * ECC_P384_INTEGER_SIZE_IN_DWORDS)
#define ECC_P384_POINT_SIZE_IN_BYTES       (2 * ECC_P384_INTEGER_SIZE_IN_BYTES)

#define ECC_P384_PRIVKEY_SIZE_IN_BYTES     (ECC_P384_INTEGER_SIZE_IN_BYTES)
#define ECC_P384_PRIVKEY_SIZE_IN_DWORDS    (ECC_P384_PRIVKEY_SIZE_IN_BYTES/sizeof(uint32))

#define ECC_P384_PUBKEY_SIZE_IN_BYTES      (ECC_P384_POINT_SIZE_IN_BYTES)
#define ECC_P384_PUBKEY_SIZE_IN_DWORDS     (ECC_P384_PUBKEY_SIZE_IN_BYTES/sizeof(uint32))

#define ECC_P384_SECRET_SIZE_IN_BYTES      (ECC_P384_INTEGER_SIZE_IN_BYTES)
#define ECC_P384_SECRET_SIZE_IN_DWORDS     (ECC_P384_SECRET_SIZE_IN_BYTES/sizeof(uint32))

#define ECC_SIGN_MAX_RETRIES               (1000)
#define ECC_GEN_PRIV_KEY_MAX_RETRIES       (100)

typedef struct {
    LwU32  ecPrivate [ECC_P384_PRIVKEY_SIZE_IN_DWORDS];
    LwU32  ecPublic  [ECC_P384_PUBKEY_SIZE_IN_DWORDS];
    LwBool  bInitialized;
} EC_CONTEXT, *PEC_CONTEXT;

/* ------------------------- Prototypes ------------------------------------- */
/**
  Performs AEAD AES-128-CTR and HMAC-SHA256 authenticated encryption on a
  data buffer and additional authenticated data (AAD).

  iv_size must be 16, otherwise FALSE is returned.
  key_size must be 16, otherwise FALSE is returned.
  tag_size must be 32, otherwise FALSE is returned.

  @param[in]   key         Pointer to the encryption key.
  @param[in]   key_size     size of the encryption key in bytes.
  @param[in]   iv          Pointer to the IV value.
  @param[in]   iv_size      size of the IV value in bytes.
  @param[in]   a_data       Pointer to the additional authenticated data (AAD).
  @param[in]   a_data_size   size of the additional authenticated data (AAD) in bytes.
  @param[in]   data_in      Pointer to the input data buffer to be encrypted.
  @param[in]   data_in_size  size of the input data buffer in bytes.
  @param[out]  tag_out      Pointer to a buffer that receives the authentication tag output.
  @param[in]   tag_size     size of the authentication tag in bytes.
  @param[out]  data_out     Pointer to a buffer that receives the encryption output.
  @param[out]  data_out_size size of the output data buffer in bytes.

  @retval TRUE   AEAD AES-GCM authenticated encryption succeeded.
  @retval FALSE  AEAD AES-GCM authenticated encryption failed.

**/
boolean aead_aes_ctr_hmac_sha_encrypt(IN const uint8 *key, IN uintn key_size,
    IN const uint8 *iv, IN uintn iv_size,
    IN const uint8 *a_data, IN uintn a_data_size,
    IN const uint8 *data_in, IN uintn data_in_size,
    OUT uint8 *tag_out, IN uintn tag_size,
    OUT uint8 *data_out, OUT uintn *data_out_size)
    GCC_ATTRIB_SECTION("imem_libspdm", "aead_aes_ctr_hmac_sha_encrypt");

/**
  Performs AEAD AES-128-CTR and HMAC-SHA256 authenticated decryption on a
  data buffer and additional authenticated data (AAD).

  iv_size must be 16, otherwise FALSE is returned.
  key_size must be 16, otherwise FALSE is returned.
  tag_size must be 32, otherwise FALSE is returned.
  If additional authenticated data verification fails, FALSE is returned.

  @param[in]   key         Pointer to the encryption key.
  @param[in]   key_size     size of the encryption key in bytes.
  @param[in]   iv          Pointer to the IV value.
  @param[in]   iv_size      size of the IV value in bytes.
  @param[in]   a_data       Pointer to the additional authenticated data (AAD).
  @param[in]   a_data_size   size of the additional authenticated data (AAD) in bytes.
  @param[in]   data_in      Pointer to the input data buffer to be decrypted.
  @param[in]   data_in_size  size of the input data buffer in bytes.
  @param[in]   tag         Pointer to a buffer that contains the authentication tag.
  @param[in]   tag_size     size of the authentication tag in bytes.
  @param[out]  data_out     Pointer to a buffer that receives the decryption output.
  @param[out]  data_out_size size of the output data buffer in bytes.

  @retval TRUE   AEAD AES-GCM authenticated decryption succeeded.
  @retval FALSE  AEAD AES-GCM authenticated decryption failed.

**/
boolean aead_aes_ctr_hmac_sha_decrypt(IN const uint8 *key, IN uintn key_size,
    IN const uint8 *iv, IN uintn iv_size,
    IN const uint8 *a_data, IN uintn a_data_size,
    IN const uint8 *data_in, IN uintn data_in_size,
    IN const uint8 *tag, IN uintn tag_size,
    OUT uint8 *data_out, OUT uintn *data_out_size)
    GCC_ATTRIB_SECTION("imem_libspdm", "aead_aes_ctr_hmac_sha_decrypt");

/**
  Derive key data using HMAC-SHA128 based KDF.

  @param[in]   key              Pointer to the user-supplied key.
  @param[in]   key_size          key size in bytes.
  @param[in]   salt             Pointer to the salt(non-secret) value.
  @param[in]   salt_size         salt size in bytes.
  @param[in]   info             Pointer to the application specific info.
  @param[in]   info_size         info size in bytes.
  @param[out]  out              Pointer to buffer to receive hkdf value.
  @param[in]   out_size          size of hkdf bytes to generate.

  @retval TRUE   Hkdf generated successfully.
  @retval FALSE  Hkdf generation failed.

**/
boolean hkdf_sha128_extract_and_expand(IN const uint8 *key, IN uintn key_size,
    IN const uint8 *salt, IN uintn salt_size,
    IN const uint8 *info, IN uintn info_size,
    OUT uint8 *out, IN uintn out_size)
    GCC_ATTRIB_SECTION("imem_libspdm", "hkdf_sha128_extract_and_expand");

/**
  Derive SHA128 HMAC-based Extract key Derivation Function (HKDF).

  @param[in]   key              Pointer to the user-supplied key.
  @param[in]   key_size          key size in bytes.
  @param[in]   salt             Pointer to the salt(non-secret) value.
  @param[in]   salt_size         salt size in bytes.
  @param[out]  prk_out           Pointer to buffer to receive hkdf value.
  @param[in]   prk_out_size       size of hkdf bytes to generate.

  @retval TRUE   Hkdf generated successfully.
  @retval FALSE  Hkdf generation failed.

**/
boolean hkdf_sha128_extract(IN const uint8 *key, IN uintn key_size,
    IN const uint8 *salt, IN uintn salt_size,
    OUT uint8 *prk_out, IN uintn prk_out_size)
    GCC_ATTRIB_SECTION("imem_libspdm", "hkdf_sha128_extract");

/**
  Derive SHA128 HMAC-based Expand key Derivation Function (HKDF).

  @param[in]   prk              Pointer to the user-supplied key.
  @param[in]   prk_size          key size in bytes.
  @param[in]   info             Pointer to the application specific info.
  @param[in]   info_size         info size in bytes.
  @param[out]  out              Pointer to buffer to receive hkdf value.
  @param[in]   out_size          size of hkdf bytes to generate.

  @retval TRUE   Hkdf generated successfully.
  @retval FALSE  Hkdf generation failed.

**/
boolean hkdf_sha128_expand(IN const uint8 *prk, IN uintn prk_size,
    IN const uint8 *info, IN uintn info_size,
    OUT uint8 *out, IN uintn out_size)
    GCC_ATTRIB_SECTION("imem_libspdm", "hkdf_sha128_expand");

/*!
 * @brief Generates an EC key pair for the NIST P-256 lwrve.
 *
 * @param[out] pPubKey   Pointer to public key, must have size of
 *                       ECC_P256_PUBKEY_SIZE_IN_DWORDS.
 * @param[out] pPrivKey  Pointer to private key, must have size of
 *                       ECC_P256_PRIVKEY_SIZE_IN_DWORDS.
 *
 * @return TRUE if key pair is successfully generated,
 *         FALSE otherwise.
 */
boolean ec_generate_ecc_key_pair(OUT uint32 *pPubKey, OUT uint32 *pPrivKey)
    GCC_ATTRIB_SECTION("imem_libspdm", "ec_generate_ecc_key_pair");

#endif // _LW_CRYPT_H_
