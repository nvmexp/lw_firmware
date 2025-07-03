/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

//
// LWE (tandrewprinz): Including LWPU copyright, as file was forked from
// libSPDM source-code, with LW modifications.
//

/*!
 * @file   lw_crypt.c
 * @brief  Fork of library/spdm_crypt_lib/crypt.c, heavily modified.
 *         Required as many functions in the library were not used,
 *         and those that are used relied on FP, which we needed to remove.
 *         See LW_REMOVE_FP for other examples.
 */

/* ------------------------- Application Includes --------------------------- */
#include <library/spdm_crypt_lib.h>

/* ------------------------- Public Functions ------------------------------- */
/**
  This function returns the SPDM hash algorithm size.

  @param  bash_hash_algo                  SPDM bash_hash_algo

  @return SPDM hash algorithm size.
**/
uint32 spdm_get_hash_size(IN uint32 bash_hash_algo)
{
    switch (bash_hash_algo) {
    case SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_256:
    case SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA3_256:
        return 32;
    case SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_384:
    case SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA3_384:
        return 48;
    case SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_512:
    case SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA3_512:
        return 64;
    }
    return 0;
}

/**
  Return cipher ID, based upon the negotiated hash algorithm.

  @param  bash_hash_algo                  SPDM bash_hash_algo

  @return hash cipher ID
**/
uintn get_spdm_hash_nid(IN uint32 bash_hash_algo)
{
    switch (bash_hash_algo) {
    case SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_256:
        return CRYPTO_NID_SHA256;
    case SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_384:
        return CRYPTO_NID_SHA384;
    case SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_512:
        return CRYPTO_NID_SHA512;
    case SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA3_256:
        return CRYPTO_NID_SHA3_256;
    case SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA3_384:
        return CRYPTO_NID_SHA3_384;
    case SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA3_512:
        return CRYPTO_NID_SHA3_512;
    }
    return CRYPTO_NID_NULL;
}

/**
  Computes the hash of a input data buffer, based upon the negotiated hash algorithm.

  This function performs the hash of a given data buffer, and return the hash value.

  @param  bash_hash_algo                 SPDM bash_hash_algo
  @param  data                         Pointer to the buffer containing the data to be hashed.
  @param  data_size                     size of data buffer in bytes.
  @param  hash_value                    Pointer to a buffer that receives the hash value.

  @retval TRUE   hash computation succeeded.
  @retval FALSE  hash computation failed.
**/
boolean spdm_hash_all(IN uint32 bash_hash_algo, IN const void *data,
              IN uintn data_size, OUT uint8 *hash_value)
{
    switch (bash_hash_algo) {
        case SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_256:
            return sha256_hash_all(data, data_size, hash_value);
        case SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_384:
            return sha384_hash_all(data, data_size, hash_value);
        default:
            break;
    }

    return FALSE;
}

/**
  This function returns the SPDM measurement hash algorithm size.

  @param  measurement_hash_algo          SPDM measurement_hash_algo

  @return SPDM measurement hash algorithm size.
  @return 0xFFFFFFFF for RAW_BIT_STREAM_ONLY.
**/
uint32 spdm_get_measurement_hash_size(IN uint32 measurement_hash_algo)
{
    switch (measurement_hash_algo) {
    case SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_256:
    case SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA3_256:
        return 32;
    case SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_384:
    case SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA3_384:
        return 48;
    case SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_512:
    case SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA3_512:
        return 64;
    case SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_RAW_BIT_STREAM_ONLY:
        return 0xFFFFFFFF;
    }
    return 0;
}

/**
  Computes the hash of a input data buffer, based upon the negotiated measurement hash algorithm.

  This function performs the hash of a given data buffer, and return the hash value.

  @param  measurement_hash_algo          SPDM measurement_hash_algo
  @param  data                         Pointer to the buffer containing the data to be hashed.
  @param  data_size                     size of data buffer in bytes.
  @param  hash_value                    Pointer to a buffer that receives the hash value.

  @retval TRUE   hash computation succeeded.
  @retval FALSE  hash computation failed.
**/
boolean spdm_measurement_hash_all(IN uint32 measurement_hash_algo,
                  IN const void *data, IN uintn data_size,
                  OUT uint8 *hash_value)
{
    switch (measurement_hash_algo) {
        case SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_256:
            return sha256_hash_all(data, data_size, hash_value);
        case SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_384:
            return sha384_hash_all(data, data_size, hash_value);
        default:
            break;
    }

    return FALSE;
}

/**
  Computes the HMAC of a input data buffer, based upon the negotiated HMAC algorithm.

  This function performs the HMAC of a given data buffer, and return the hash value.

  @param  bash_hash_algo                 SPDM bash_hash_algo
  @param  data                         Pointer to the buffer containing the data to be HMACed.
  @param  data_size                     size of data buffer in bytes.
  @param  key                          Pointer to the user-supplied key.
  @param  key_size                      key size in bytes.
  @param  hash_value                    Pointer to a buffer that receives the HMAC value.

  @retval TRUE   HMAC computation succeeded.
  @retval FALSE  HMAC computation failed.
**/
boolean spdm_hmac_all(IN uint32 base_hash_algo, IN const void *data,
              IN uintn data_size, IN const uint8 *key,
              IN uintn key_size, OUT uint8 *hmac_value)
{
    switch (base_hash_algo) {
        case SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_256:
            return hmac_sha256_all(data, data_size, key, key_size, hmac_value);
        case SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_384:
            return hmac_sha384_all(data, data_size, key, key_size, hmac_value);
        default:
            break;
    }

    return FALSE;
}

/**
  Derive HMAC-based Expand key Derivation Function (HKDF) Expand, based upon the negotiated HKDF algorithm.

  @param  bash_hash_algo                 SPDM bash_hash_algo
  @param  prk                          Pointer to the user-supplied key.
  @param  prk_size                      key size in bytes.
  @param  info                         Pointer to the application specific info.
  @param  info_size                     info size in bytes.
  @param  out                          Pointer to buffer to receive hkdf value.
  @param  out_size                      size of hkdf bytes to generate.

  @retval TRUE   Hkdf generated successfully.
  @retval FALSE  Hkdf generation failed.
**/
boolean spdm_hkdf_expand(IN uint32 bash_hash_algo, IN const uint8 *prk,
             IN uintn prk_size, IN const uint8 *info,
             IN uintn info_size, OUT uint8 *out, IN uintn out_size)
{
    switch (bash_hash_algo) {
        case  SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_384:
            return hkdf_sha384_expand(prk, prk_size, info, info_size, out,
                                      out_size);
        default:
            break;
    }
    return FALSE;
}

/**
  This function returns the SPDM asymmetric algorithm size.

  @param  base_asym_algo                 SPDM base_asym_algo

  @return SPDM asymmetric algorithm size.
**/
uint32 spdm_get_asym_signature_size(IN uint32 base_asym_algo)
{
    switch (base_asym_algo) {
    case SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSASSA_2048:
    case SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSAPSS_2048:
        return 256;
    case SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSASSA_3072:
    case SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSAPSS_3072:
        return 384;
    case SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSASSA_4096:
    case SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSAPSS_4096:
        return 512;
    case SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P256:
        return 32 * 2;
    case SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P384:
        return 48 * 2;
    case SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P521:
        return 66 * 2;
    }
    return 0;
}

/**
  Release the specified asymmetric context,
  based upon negotiated asymmetric algorithm.

  @param  base_asym_algo                 SPDM base_asym_algo
  @param  context                      Pointer to the asymmetric context to be released.
**/
void spdm_asym_free(IN uint32 base_asym_algo, IN void *context)
{
    switch (base_asym_algo) {
        case SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P384:
            return ec_free(context);
        default:
            break;
    }

    return;
}

/**
  Verifies the asymmetric signature,
  based upon negotiated asymmetric algorithm.

  @param  base_asym_algo                 SPDM base_asym_algo
  @param  bash_hash_algo                 SPDM bash_hash_algo
  @param  context                      Pointer to asymmetric context for signature verification.
  @param  message                      Pointer to octet message to be checked (before hash).
  @param  message_size                  size of the message in bytes.
  @param  signature                    Pointer to asymmetric signature to be verified.
  @param  sig_size                      size of signature in bytes.

  @retval  TRUE   Valid asymmetric signature.
  @retval  FALSE  Invalid asymmetric signature or invalid asymmetric context.
**/
boolean spdm_asym_verify(IN uint32 base_asym_algo, IN uint32 bash_hash_algo,
             IN void *context, IN const uint8 *message,
             IN uintn message_size, IN const uint8 *signature,
             IN uintn sig_size)
{
    uint8 message_hash[MAX_HASH_SIZE];
    uintn hash_size;
    boolean result;
    uintn hash_nid;

    hash_nid = get_spdm_hash_nid(bash_hash_algo);

    switch (base_asym_algo) {
        case SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P384:
            hash_size = spdm_get_hash_size(bash_hash_algo);
            result = spdm_hash_all(bash_hash_algo, message, message_size,
                            message_hash);
            if (!result) {
                return FALSE;
            }
            return ecdsa_verify(context, hash_nid, message_hash,
                            hash_size, signature, sig_size);
        default:
            break;
    }

    return FALSE;
}

/**
  Carries out the signature generation.

  If the signature buffer is too small to hold the contents of signature, FALSE
  is returned and sig_size is set to the required buffer size to obtain the signature.

  @param  base_asym_algo                 SPDM base_asym_algo
  @param  bash_hash_algo                 SPDM bash_hash_algo
  @param  context                      Pointer to asymmetric context for signature generation.
  @param  message                      Pointer to octet message to be signed (before hash).
  @param  message_size                  size of the message in bytes.
  @param  signature                    Pointer to buffer to receive signature.
  @param  sig_size                      On input, the size of signature buffer in bytes.
                                       On output, the size of data returned in signature buffer in bytes.

  @retval  TRUE   signature successfully generated.
  @retval  FALSE  signature generation failed.
  @retval  FALSE  sig_size is too small.
**/
boolean spdm_asym_sign(IN uint32 base_asym_algo, IN uint32 bash_hash_algo,
               IN void *context, IN const uint8 *message,
               IN uintn message_size, OUT uint8 *signature,
               IN OUT uintn *sig_size)
{
    uint8 message_hash[MAX_HASH_SIZE];
    uintn hash_size;
    boolean result;
    uintn hash_nid;

    hash_nid = get_spdm_hash_nid(bash_hash_algo);

    switch (base_asym_algo) {
        case SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P384:
            hash_size = spdm_get_hash_size(bash_hash_algo);
            result = spdm_hash_all(bash_hash_algo, message, message_size,
                           message_hash);
            if (!result) {
                return FALSE;
            }
            return ecdsa_sign(context, hash_nid, message_hash, hash_size,
                 signature, sig_size);
        default:
            break;
    }

    return FALSE;
}


/**
  This function returns the SPDM DHE algorithm key size.

  @param  dhe_named_group                SPDM dhe_named_group

  @return SPDM DHE algorithm key size.
**/
uint32 spdm_get_dhe_pub_key_size(IN uint16 dhe_named_group)
{
    switch (dhe_named_group) {
    case SPDM_ALGORITHMS_DHE_NAMED_GROUP_FFDHE_2048:
        return 256;
    case SPDM_ALGORITHMS_DHE_NAMED_GROUP_FFDHE_3072:
        return 384;
    case SPDM_ALGORITHMS_DHE_NAMED_GROUP_FFDHE_4096:
        return 512;
    case SPDM_ALGORITHMS_DHE_NAMED_GROUP_SECP_256_R1:
        return 32 * 2;
    case SPDM_ALGORITHMS_DHE_NAMED_GROUP_SECP_384_R1:
        return 48 * 2;
    case SPDM_ALGORITHMS_DHE_NAMED_GROUP_SECP_521_R1:
        return 66 * 2;
    }
    return 0;
}

/**
  Allocates and Initializes one Diffie-Hellman Ephemeral (DHE) context for subsequent use,
  based upon negotiated DHE algorithm.

  @param  dhe_named_group                SPDM dhe_named_group

  @return  Pointer to the Diffie-Hellman context that has been initialized.
**/
void *spdm_dhe_new(IN uint16 dhe_named_group)
{
    uintn nid;

    switch (dhe_named_group) {
        case SPDM_ALGORITHMS_DHE_NAMED_GROUP_SECP_384_R1:
            nid = CRYPTO_NID_SECP384R1;
            return ec_new_by_nid(nid);
    }

    return FALSE;
}

/**
  Release the specified DHE context,
  based upon negotiated DHE algorithm.

  @param  dhe_named_group                SPDM dhe_named_group
  @param  context                      Pointer to the DHE context to be released.
**/
void spdm_dhe_free(IN uint16 dhe_named_group, IN void *context)
{
    switch (dhe_named_group) {
        case SPDM_ALGORITHMS_DHE_NAMED_GROUP_SECP_384_R1:
            ec_free(context);
            break;
        default:
            break;
    }

    return;
}

/**
  Generates DHE public key,
  based upon negotiated DHE algorithm.

  This function generates random secret exponent, and computes the public key, which is
  returned via parameter public_key and public_key_size. DH context is updated accordingly.
  If the public_key buffer is too small to hold the public key, FALSE is returned and
  public_key_size is set to the required buffer size to obtain the public key.

  @param  dhe_named_group                SPDM dhe_named_group
  @param  context                      Pointer to the DHE context.
  @param  public_key                    Pointer to the buffer to receive generated public key.
  @param  public_key_size                On input, the size of public_key buffer in bytes.
                                       On output, the size of data returned in public_key buffer in bytes.

  @retval TRUE   DHE public key generation succeeded.
  @retval FALSE  DHE public key generation failed.
  @retval FALSE  public_key_size is not large enough.
**/
boolean spdm_dhe_generate_key(IN uint16 dhe_named_group, IN OUT void *context,
                  OUT uint8 *public_key,
                  IN OUT uintn *public_key_size)
{
    switch (dhe_named_group) {
        case SPDM_ALGORITHMS_DHE_NAMED_GROUP_SECP_384_R1:
            return ec_generate_key(context, public_key, public_key_size);
        default:
            break;
    }

    return FALSE;
}

/**
  Computes exchanged common key,
  based upon negotiated DHE algorithm.

  Given peer's public key, this function computes the exchanged common key, based on its own
  context including value of prime modulus and random secret exponent.

  @param  dhe_named_group                SPDM dhe_named_group
  @param  context                      Pointer to the DHE context.
  @param  peer_public_key                Pointer to the peer's public key.
  @param  peer_public_key_size            size of peer's public key in bytes.
  @param  key                          Pointer to the buffer to receive generated key.
  @param  key_size                      On input, the size of key buffer in bytes.
                                       On output, the size of data returned in key buffer in bytes.

  @retval TRUE   DHE exchanged key generation succeeded.
  @retval FALSE  DHE exchanged key generation failed.
  @retval FALSE  key_size is not large enough.
**/
boolean spdm_dhe_compute_key(IN uint16 dhe_named_group, IN OUT void *context,
                 IN const uint8 *peer_public,
                 IN uintn peer_public_size, OUT uint8 *key,
                 IN OUT uintn *key_size)
{
    switch (dhe_named_group) {
        case SPDM_ALGORITHMS_DHE_NAMED_GROUP_SECP_384_R1:
            return ec_compute_key(context, peer_public, peer_public_size, key,
                        key_size);
        default:
            break;
    }

    return FALSE;
}

/**
  This function returns the SPDM AEAD algorithm key size.

  @param  aead_cipher_suite              SPDM aead_cipher_suite

  @return SPDM AEAD algorithm key size.
**/
uint32 spdm_get_aead_key_size(IN uint16 aead_cipher_suite)
{
    switch (aead_cipher_suite) {
    case SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_128_GCM:
        return 16;
    case SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_256_GCM:
        return 32;
    case SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_CHACHA20_POLY1305:
        return 32;
    }
    return 0;
}

/**
  This function returns the SPDM AEAD algorithm iv size.

  @param  aead_cipher_suite              SPDM aead_cipher_suite

  @return SPDM AEAD algorithm iv size.
**/
uint32 spdm_get_aead_iv_size(IN uint16 aead_cipher_suite)
{
    switch (aead_cipher_suite) {
    case SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_128_GCM:
        return 12;
    case SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_256_GCM:
        return 12;
    case SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_CHACHA20_POLY1305:
        return 12;
    }
    return 0;
}

/**
  This function returns the SPDM AEAD algorithm tag size.

  @param  aead_cipher_suite              SPDM aead_cipher_suite

  @return SPDM AEAD algorithm tag size.
**/
uint32 spdm_get_aead_tag_size(IN uint16 aead_cipher_suite)
{
    switch (aead_cipher_suite) {
    case SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_128_GCM:
        return 16;
    case SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_256_GCM:
        return 16;
    case SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_CHACHA20_POLY1305:
        return 16;
    }
    return 0;
}

/**
  This function returns the SPDM AEAD algorithm block size.

  @param  aead_cipher_suite              SPDM aead_cipher_suite

  @return SPDM AEAD algorithm block size.
**/
uint32 spdm_get_aead_block_size(IN uint16 aead_cipher_suite)
{
    switch (aead_cipher_suite) {
    case SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_128_GCM:
        return 16;
    case SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_256_GCM:
        return 16;
    case SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_CHACHA20_POLY1305:
        return 16;
    }
    return 0;
}

/**
  Performs AEAD authenticated encryption on a data buffer and additional authenticated data (AAD),
  based upon negotiated AEAD algorithm.

  @param  aead_cipher_suite              SPDM aead_cipher_suite
  @param  key                          Pointer to the encryption key.
  @param  key_size                      size of the encryption key in bytes.
  @param  iv                           Pointer to the IV value.
  @param  iv_size                       size of the IV value in bytes.
  @param  a_data                        Pointer to the additional authenticated data (AAD).
  @param  a_data_size                    size of the additional authenticated data (AAD) in bytes.
  @param  data_in                       Pointer to the input data buffer to be encrypted.
  @param  data_in_size                   size of the input data buffer in bytes.
  @param  tag_out                       Pointer to a buffer that receives the authentication tag output.
  @param  tag_size                      size of the authentication tag in bytes.
  @param  data_out                      Pointer to a buffer that receives the encryption output.
  @param  data_out_size                  size of the output data buffer in bytes.

  @retval TRUE   AEAD authenticated encryption succeeded.
  @retval FALSE  AEAD authenticated encryption failed.
**/
boolean spdm_aead_encryption(IN uint16 aead_cipher_suite, IN const uint8 *key,
                 IN uintn key_size, IN const uint8 *iv,
                 IN uintn iv_size, IN const uint8 *a_data,
                 IN uintn a_data_size, IN const uint8 *data_in,
                 IN uintn data_in_size, OUT uint8 *tag_out,
                 IN uintn tag_size, OUT uint8 *data_out,
                 OUT uintn *data_out_size)
{
    switch (aead_cipher_suite) {
        case  SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_256_GCM:
              return aead_aes_gcm_encrypt(key, key_size, iv, iv_size, a_data, a_data_size, data_in,
                                          data_in_size, tag_out, tag_size, data_out, data_out_size);
            break;
        default:
            break;
    }

    return FALSE;
}

/**
  Performs AEAD authenticated decryption on a data buffer and additional authenticated data (AAD),
  based upon negotiated AEAD algorithm.

  @param  aead_cipher_suite              SPDM aead_cipher_suite
  @param  key                          Pointer to the encryption key.
  @param  key_size                      size of the encryption key in bytes.
  @param  iv                           Pointer to the IV value.
  @param  iv_size                       size of the IV value in bytes.
  @param  a_data                        Pointer to the additional authenticated data (AAD).
  @param  a_data_size                    size of the additional authenticated data (AAD) in bytes.
  @param  data_in                       Pointer to the input data buffer to be decrypted.
  @param  data_in_size                   size of the input data buffer in bytes.
  @param  tag                          Pointer to a buffer that contains the authentication tag.
  @param  tag_size                      size of the authentication tag in bytes.
  @param  data_out                      Pointer to a buffer that receives the decryption output.
  @param  data_out_size                  size of the output data buffer in bytes.

  @retval TRUE   AEAD authenticated decryption succeeded.
  @retval FALSE  AEAD authenticated decryption failed.
**/
boolean spdm_aead_decryption(IN uint16 aead_cipher_suite, IN const uint8 *key,
                 IN uintn key_size, IN const uint8 *iv,
                 IN uintn iv_size, IN const uint8 *a_data,
                 IN uintn a_data_size, IN const uint8 *data_in,
                 IN uintn data_in_size, IN const uint8 *tag,
                 IN uintn tag_size, OUT uint8 *data_out,
                 OUT uintn *data_out_size)
{
    switch (aead_cipher_suite) {
        case  SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_256_GCM:
              return aead_aes_gcm_decrypt(key, key_size, iv, iv_size, a_data, a_data_size, data_in,
                                          data_in_size, tag, tag_size, data_out, data_out_size);
            break;
        default:
            break;
    }

    return FALSE;
}

/**
  Generates a random byte stream of the specified size.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  size                         size of random bytes to generate.
  @param  rand                         Pointer to buffer to receive random value.
**/
void spdm_get_random_number(IN uintn size, OUT uint8 *rand)
{
    random_bytes(rand, size);

    return;
}
