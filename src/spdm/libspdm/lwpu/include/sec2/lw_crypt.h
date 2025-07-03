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
 * @file   lw_crypt.h
 * @brief  Function declarations for custom crypto functionality by LWPU, as
 *         well as defines and structs required for implementation.
 */

#ifndef _LW_CRYPT_H_
#define _LW_CRYPT_H_

/* ------------------------ Macros and Defines ----------------------------- */
#define ECC_P256_INTEGER_SIZE_IN_DWORDS    (8)
#define ECC_P256_INTEGER_SIZE_IN_BYTES     (4 * ECC_P256_INTEGER_SIZE_IN_DWORDS)
#define ECC_P256_POINT_SIZE_IN_DWORDS      (2 * ECC_P256_INTEGER_SIZE_IN_DWORDS)
#define ECC_P256_POINT_SIZE_IN_BYTES       (2 * ECC_P256_INTEGER_SIZE_IN_BYTES)

#define ECC_P256_PRIVKEY_SIZE_IN_BYTES     (ECC_P256_INTEGER_SIZE_IN_BYTES)
#define ECC_P256_PRIVKEY_SIZE_IN_DWORDS    (ECC_P256_PRIVKEY_SIZE_IN_BYTES/sizeof(uint32))

#define ECC_P256_PUBKEY_SIZE_IN_BYTES      (ECC_P256_POINT_SIZE_IN_BYTES)
#define ECC_P256_PUBKEY_SIZE_IN_DWORDS     (ECC_P256_PUBKEY_SIZE_IN_BYTES/sizeof(uint32))

#define ECC_P256_SECRET_SIZE_IN_BYTES      (ECC_P256_POINT_SIZE_IN_BYTES)
#define ECC_P256_SECRET_SIZE_IN_DWORDS     (ECC_P256_SECRET_SIZE_IN_BYTES/sizeof(uint32))

#define ECC_SIGN_MAX_RETRIES               (1000)
#define ECC_GEN_PRIV_KEY_MAX_RETRIES       (100)

// Protected by g_ecContextMutex, declared in lw_sec2_rtos.h and defined in lw_ecc.c.
typedef struct {
  LwU32  ecPrivate[ECC_P256_PRIVKEY_SIZE_IN_DWORDS];
  LwU32  ecPublic[ECC_P256_PUBKEY_SIZE_IN_DWORDS];
  LwBool bInitialized;
} EC_CONTEXT;

#define AES_128_CTR_IV_SIZE_IN_BYTES       (12)

/* ------------------------- Prototypes ------------------------------------- */
/**
  Initialize semaphore used to protect ec_context for ECC implementations.
  
  @note Must be called before any EC operations are used, the behavior
  is undefined otherwise.
  
  @return TRUE if mutex is successfully initialized,
          FALSE otherwise.
**/
boolean spdm_ec_context_mutex_initialize(void)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_ec_context_mutex_initialize");

/*!
  @brief Generates an EC key pair for the NIST P-256 lwrve.
  
  @note Outputs keys in big-endian format.
  
  @param[out] pPubKey   Pointer to public key, must have size of
                        ECC_P256_PUBKEY_SIZE_IN_DWORDS.
  @param[out] pPrivKey  Pointer to private key, must have size of
                        ECC_P256_PRIVKEY_SIZE_IN_DWORDS.
  
  @return TRUE if key pair is successfully generated,
          FALSE otherwise.
 */
boolean ec_generate_ecc_key_pair(OUT uint32 *pPubKey, OUT uint32 *pPrivKey)
    GCC_ATTRIB_SECTION("imem_libspdm", "ec_generate_ecc_key_pair");

/**
  Performs AEAD AES-128-CTR and HMAC-SHA256 authenticated encryption on a
  data buffer and additional authenticated data (AAD).

  iv_size must be 12, otherwise FALSE is returned.
  key_size must be 16, otherwise FALSE is returned.
  tag_size must be 32, otherwise FALSE is returned.

  @param[in]  key            Pointer to the encryption key.
  @param[in]  key_size       Size of the encryption key in bytes.
  @param[in]  iv             Pointer to the IV value.
  @param[in]  iv_size        Size of the IV value in bytes.
  @param[in]  a_data         Pointer to the additional authenticated data (AAD).
  @param[in]  a_data_size    Size of the additional authenticated data (AAD) in bytes.
  @param[in]  data_in        Pointer to the input data buffer to be encrypted.
  @param[in]  data_in_size   Size of the input data buffer in bytes.
  @param[out] tag_out        Pointer to a buffer that receives the authentication tag output.
  @param[in]  tag_size       Size of the authentication tag in bytes.
  @param[out] data_out       Pointer to a buffer that receives the encryption output.
  @param[out] data_out_size  Size of the output data buffer in bytes.

  @retval TRUE   AEAD AES-CTR and HMAC-SHA256 authenticated encryption succeeded.
  @retval FALSE  AEAD AES-CTR and HMAC-SHA256 authenticated encryption failed.

**/
boolean aead_aes_ctr_hmac_sha_encrypt(IN const uint8 *key, IN uintn key_size,
    IN const uint8 *iv, IN uintn iv_size, IN const uint8 *a_data, IN uintn a_data_size,
    IN const uint8 *data_in, IN uintn data_in_size, OUT uint8 *tag_out, IN uintn tag_size,
    OUT uint8 *data_out, OUT uintn *data_out_size)
    GCC_ATTRIB_SECTION("imem_libspdm", "aead_aes_ctr_hmac_sha_encrypt");

/**
  Performs AEAD AES-128-CTR and HMAC-SHA256 authenticated decryption on a
  data buffer and additional authenticated data (AAD).

  iv_size must be 12, otherwise FALSE is returned.
  key_size must be 16, otherwise FALSE is returned.
  tag_size must be 32, otherwise FALSE is returned.
  If additional authenticated data verification fails, FALSE is returned.

  @param[in]  key            Pointer to the encryption key.
  @param[in]  key_size       Size of the encryption key in bytes.
  @param[in]  iv             Pointer to the IV value.
  @param[in]  iv_size        Size of the IV value in bytes.
  @param[in]  a_data         Pointer to the additional authenticated data (AAD).
  @param[in]  a_data_size    Size of the additional authenticated data (AAD) in bytes.
  @param[in]  data_in        Pointer to the input data buffer to be decrypted.
  @param[in]  data_in_size   Size of the input data buffer in bytes.
  @param[in]  tag            Pointer to a buffer that contains the authentication tag.
  @param[in]  tag_size       Size of the authentication tag in bytes.
  @param[out] data_out       Pointer to a buffer that receives the decryption output.
  @param[out] data_out_size  Size of the output data buffer in bytes.

  @retval TRUE   AEAD AES-CTR and HMAC-SHA256 authenticated decryption succeeded.
  @retval FALSE  AEAD AES-CTR and HMAC-SHA256 authenticated decryption failed.

**/
boolean aead_aes_ctr_hmac_sha_decrypt(IN const uint8 *key, IN uintn key_size,
    IN const uint8 *iv, IN uintn iv_size, IN const uint8 *a_data, IN uintn a_data_size,
    IN const uint8 *data_in, IN uintn data_in_size, IN const uint8 *tag, IN uintn tag_size,
    OUT uint8 *data_out, OUT uintn *data_out_size)
    GCC_ATTRIB_SECTION("imem_libspdm", "aead_aes_ctr_hmac_sha_decrypt");

/**
  Computes the SHA-1 message digest of a input data buffer.

  IMPORTANT NOTE: SHA-1 is considerered inselwre, and should not be use as a secure
  hash.   It is only included here for simple hashing uses, such as generating fields in
  a x509 certificate as dictated by standard.
  
  data must be not NULL, otherwise FALSE is returned.
  data_size must not be 0, otherwise FALSE is returned.
  hash_value must be not NULL, otherwise FALSE is returned.

  This function performs the SHA-1 message digest of a given data buffer, and places
  the digest value into the specified memory.

  If this interface is not supported, then return FALSE.

  @param[in]   data        Pointer to the buffer containing the data to be hashed.
  @param[in]   data_size   Size of data buffer in bytes.
  @param[out]  hash_value  Pointer to a buffer that receives the SHA-1 digest
                           value (20 bytes).

  @retval TRUE   SHA-1 digest computation succeeded.
  @retval FALSE  SHA-1 digest computation failed.
  @retval FALSE  This interface is not supported.
**/
boolean sha1_hash_all(IN const void *data, IN uintn data_size, OUT uint8 *hash_value)
    GCC_ATTRIB_SECTION("imem_spdm", "sha1_hash_all");

#endif // _LW_CRYPT_H_
