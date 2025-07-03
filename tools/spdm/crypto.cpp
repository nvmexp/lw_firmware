/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Including libspdm copyright, as functions & descriptions were copied from libspdm-source.

/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

#include "spdm_cppintfc.h"

// CryptoPP includes
#include <aes.h>
#include <eccrypto.h>
#include <ecp.h>
#include <filters.h>
#include <gcm.h>
#include <hex.h>
#include <hkdf.h>
#include <hmac.h>
#include <sha.h>
#include <oids.h>
#include <osrng.h>

/**
  Performs AEAD AES-GCM authenticated encryption on a data buffer and additional authenticated data (AAD).

  iv_size must be 12, otherwise FALSE is returned.
  key_size must be 16, 24 or 32, otherwise FALSE is returned.
  tag_size must be 12, 13, 14, 15, 16, otherwise FALSE is returned.

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
boolean aead_aes_gcm_encrypt(IN const uint8 *key, IN uintn key_size,
                             IN const uint8 *iv, IN uintn iv_size,
                             IN const uint8 *a_data, IN uintn a_data_size,
                             IN const uint8 *data_in, IN uintn data_in_size,
                             OUT uint8 *tag_out, IN uintn tag_size,
                             OUT uint8 *data_out, OUT uintn *data_out_size)
{
    try
    {
        CryptoPP::GCM<CryptoPP::AES, CryptoPP::GCM_64K_Tables>::Encryption e;
        e.SetKeyWithIV(key, key_size, iv, iv_size);

        CryptoPP::AuthenticatedEncryptionFilter ef(e, new CryptoPP::ByteQueue(),
                                                   false, tag_size);

        ef.ChannelPut(CryptoPP::AAD_CHANNEL, a_data, a_data_size);
        ef.ChannelMessageEnd(CryptoPP::AAD_CHANNEL);

        ef.ChannelPut(CryptoPP::DEFAULT_CHANNEL, data_in, data_in_size);
        *data_out_size = ef.Get(data_out, a_data_size + data_in_size);

        ef.ChannelMessageEnd(CryptoPP::DEFAULT_CHANNEL);
        ef.Get(tag_out, tag_size);

        return true;
    }
    catch (CryptoPP::Exception& e)
    {
        (void) e;
        return false;
    }
}

/**
  Performs AEAD AES-GCM authenticated decryption on a data buffer and additional authenticated data (AAD).

  iv_size must be 12, otherwise FALSE is returned.
  key_size must be 16, 24 or 32, otherwise FALSE is returned.
  tag_size must be 12, 13, 14, 15, 16, otherwise FALSE is returned.
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
boolean aead_aes_gcm_decrypt(IN const uint8 *key, IN uintn key_size,
                             IN const uint8 *iv, IN uintn iv_size,
                             IN const uint8 *a_data, IN uintn a_data_size,
                             IN const uint8 *data_in, IN uintn data_in_size,
                             IN const uint8 *tag, IN uintn tag_size,
                             OUT uint8 *data_out, OUT uintn *data_out_size)
{
    try
    {
        CryptoPP::GCM<CryptoPP::AES, CryptoPP::GCM_64K_Tables>::Decryption d;
        d.SetKeyWithIV(key, key_size, iv, iv_size);

        CryptoPP::AuthenticatedDecryptionFilter df(d, new CryptoPP::ByteQueue(),
                CryptoPP::AuthenticatedDecryptionFilter::MAC_AT_BEGIN |
                CryptoPP::AuthenticatedDecryptionFilter::THROW_EXCEPTION, tag_size);

        df.ChannelPut(CryptoPP::DEFAULT_CHANNEL, tag, tag_size);

        df.ChannelPut(CryptoPP::AAD_CHANNEL, a_data, a_data_size);
        df.ChannelMessageEnd(CryptoPP::AAD_CHANNEL);

        df.ChannelPut(CryptoPP::DEFAULT_CHANNEL, data_in, data_in_size);
        df.ChannelMessageEnd(CryptoPP::DEFAULT_CHANNEL);

        *data_out_size = df.Get(data_out, a_data_size + data_in_size);

        return true;
    }
    catch (CryptoPP::Exception& e)
    {
        (void) e;
        return false;
    }
}

/**
  Performs AEAD AES-128-CTR and HMAC-SHA256 authenticated encryption on a
  data buffer and additional authenticated data (AAD). The HMAC tag is
  callwlated on the concatenation of AAD and the encrypted data buffer,
  (AAD || encrypted data). The same key is used for AES encryption and
  for HMAC.

  At least one of the data buffer or AAD must be specified in order to
  perform AEAD. If only the data buffer is specified, the tag is callwlated
  without AAD. If only AAD is specified, the tag is callwlated with only AAD,
  and no encryption is performed.

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
boolean aead_aes_ctr_encrypt_aead(IN const uint8 *key, IN uintn key_size,
                 IN const uint8 *iv, IN uintn iv_size,
                 IN const uint8 *a_data, IN uintn a_data_size,
                 IN const uint8 *data_in, IN uintn data_in_size,
                 OUT uint8 *tag_out, IN uintn tag_size,
                 OUT uint8 *data_out, OUT uintn *data_out_size)
{
    try
    {
        CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption e;
        e.SetKeyWithIV(key, key_size, iv, iv_size);
        CryptoPP::StreamTransformationFilter ef(e, new CryptoPP::ByteQueue());

        ef.Put(data_in, data_in_size);
        *data_out_size = ef.Get(data_out, data_in_size);

        CryptoPP::HMAC<CryptoPP::SHA256> hmac(key, key_size);
        hmac.Update(reinterpret_cast<unsigned char const *>(a_data), a_data_size);
        hmac.Update(reinterpret_cast<unsigned char const *>(data_out), *data_out_size);
        hmac.Final(tag_out);

        return true;
    }
    catch (CryptoPP::Exception& e)
    {
        (void) e;
        return false;
    }
}

/**
  Performs AEAD AES-128-CTR and HMAC-SHA256 authenticated decryption on a
  data buffer and additional authenticated data (AAD). The HMAC tag is
  callwlated on the concatenation of AAD and the encrypted data buffer,
  (AAD || encrypted data). The same key is used for AES encryption and
  for HMAC.

  At least one of the data buffer or AAD must be specified in order to
  perform AEAD. If only the data buffer is specified, the tag is callwlated
  without AAD. If only AAD is specified, the tag is callwlated with only AAD,
  and no decryption is performed.

  Compares the expected input tag buffer with the callwlated tag, failing if
  they do not match.

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
boolean aead_aes_ctr_decrypt_aead(IN const uint8 *key, IN uintn key_size,
    IN const uint8 *iv, IN uintn iv_size, IN const uint8 *a_data, IN uintn a_data_size,
    IN const uint8 *data_in, IN uintn data_in_size, IN const uint8 *tag, IN uintn tag_size,
    OUT uint8 *data_out, OUT uintn *data_out_size)
{
    try
    {
        uint8_t tag_ref[tag_size];
        CryptoPP::HMAC<CryptoPP::SHA256> hmac(key, key_size);
        hmac.Update(reinterpret_cast<unsigned char const *>(a_data), a_data_size);
        hmac.Update(reinterpret_cast<unsigned char const *>(data_in), data_in_size);
        hmac.Final(tag_ref);

        CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption d;
        d.SetKeyWithIV(key, key_size, iv, iv_size);
        CryptoPP::StreamTransformationFilter df(d, new CryptoPP::ByteQueue());

        if (memcmp(tag, tag_ref, tag_size) != 0)
            return false;

        df.Put(data_in, data_in_size);
        df.MessageEnd();

        *data_out_size = df.Get(data_out, data_in_size);

        return true;
    }
    catch (CryptoPP::Exception& e)
    {
        (void) e;
        return false;
    }
}

/**
  Computes the SHA-384 message digest of a input data buffer.

  This function performs the SHA-384 message digest of a given data buffer, and places
  the digest value into the specified memory.

  If this interface is not supported, then return FALSE.

  @param[in]   data        Pointer to the buffer containing the data to be hashed.
  @param[in]   data_size    size of data buffer in bytes.
  @param[out]  hash_value   Pointer to a buffer that receives the SHA-384 digest
                           value (48 bytes).

  @retval TRUE   SHA-384 digest computation succeeded.
  @retval FALSE  SHA-384 digest computation failed.
  @retval FALSE  This interface is not supported.

**/
boolean sha384_hash_all(IN const void *data, IN uintn data_size,
            OUT uint8 *hash_value)
{
    try
    {
        if (hash_value == NULL)
        {
            return false;
        }

        if (data == NULL && data_size != 0)
        {
            return false;
        }

        CryptoPP::SHA384 hash;
        hash.Update(reinterpret_cast<unsigned char const *>(data), data_size);
        hash.Final(hash_value);

        return true;
    }
    catch (CryptoPP::Exception& e)
    {
        (void) e;
        return false;
    }
}

/**
  Computes the HMAC-SHA384 digest of a input data buffer.

  This function performs the HMAC-SHA384 digest of a given data buffer, and places
  the digest value into the specified memory.

  If this interface is not supported, then return FALSE.

  @param[in]   data        Pointer to the buffer containing the data to be digested.
  @param[in]   data_size    size of data buffer in bytes.
  @param[in]   key         Pointer to the user-supplied key.
  @param[in]   key_size     key size in bytes.
  @param[out]  hash_value   Pointer to a buffer that receives the HMAC-SHA384 digest
                           value (48 bytes).

  @retval TRUE   HMAC-SHA384 digest computation succeeded.
  @retval FALSE  HMAC-SHA384 digest computation failed.
  @retval FALSE  This interface is not supported.

**/
boolean hmac_sha384_all(IN const void *data, IN uintn data_size,
            IN const uint8 *key, IN uintn key_size,
            OUT uint8 *hmac_value)
{
    try
    {
        if (hmac_value == NULL)
        {
            return false;
        }

        if (data == NULL && data_size != 0)
        {
            return false;
        }

        if ((key == NULL) && (key_size != 0))
        {
            return false;
        }

        CryptoPP::HMAC<CryptoPP::SHA384> hmac(key, key_size);
        hmac.Update(reinterpret_cast<unsigned char const *>(data), data_size);
        hmac.Final(hmac_value);

        return true;
    }
    catch (CryptoPP::Exception& e)
    {
        (void) e;
        return false;
    }
}

/**
  Computes the SHA-256 message digest of a input data buffer.

  This function performs the SHA-256 message digest of a given data buffer, and places
  the digest value into the specified memory.

  If this interface is not supported, then return FALSE.

  @param[in]   data        Pointer to the buffer containing the data to be hashed.
  @param[in]   data_size    size of data buffer in bytes.
  @param[out]  hash_value   Pointer to a buffer that receives the SHA-256 digest
                           value (32 bytes).

  @retval TRUE   SHA-256 digest computation succeeded.
  @retval FALSE  SHA-256 digest computation failed.
  @retval FALSE  This interface is not supported.

**/
boolean sha256_hash_all(IN const void *data, IN uintn data_size,
            OUT uint8 *hash_value)
{
    try
    {
        if (hash_value == NULL)
        {
            return false;
        }

        if (data == NULL && data_size != 0)
        {
            return false;
        }

        CryptoPP::SHA256 hash;
        hash.Update(reinterpret_cast<unsigned char const *>(data), data_size);
        hash.Final(hash_value);

        return true;
    }
    catch (CryptoPP::Exception& e)
    {
        (void) e;
        return false;
    }
}

/**
  Computes the HMAC-SHA256 digest of a input data buffer.

  This function performs the HMAC-SHA256 digest of a given data buffer, and places
  the digest value into the specified memory.

  If this interface is not supported, then return FALSE.

  @param[in]   data        Pointer to the buffer containing the data to be digested.
  @param[in]   data_size    size of data buffer in bytes.
  @param[in]   key         Pointer to the user-supplied key.
  @param[in]   key_size     key size in bytes.
  @param[out]  hash_value   Pointer to a buffer that receives the HMAC-SHA256 digest
                           value (32 bytes).

  @retval TRUE   HMAC-SHA256 digest computation succeeded.
  @retval FALSE  HMAC-SHA256 digest computation failed.
  @retval FALSE  This interface is not supported.

**/
boolean hmac_sha256_all(IN const void *data, IN uintn data_size,
            IN const uint8 *key, IN uintn key_size,
            OUT uint8 *hmac_value)
{
    try
    {
        if (hmac_value == NULL)
        {
            return false;
        }

        if (data == NULL && data_size != 0)
        {
            return false;
        }

        if ((key == NULL) && (key_size != 0))
        {
            return false;
        }

        CryptoPP::HMAC<CryptoPP::SHA256> hmac(key, key_size);
        hmac.Update(reinterpret_cast<unsigned char const *>(data), data_size);
        hmac.Final(hmac_value);

        return true;
    }
    catch (CryptoPP::Exception& e)
    {
        (void) e;
        return false;
    }
}

/**
  Derive SHA256 HMAC-based Expand key Derivation Function (HKDF).

  @param[in]   prk              Pointer to the user-supplied key.
  @param[in]   prk_size          key size in bytes.
  @param[in]   info             Pointer to the application specific info.
  @param[in]   info_size         info size in bytes.
  @param[out]  out              Pointer to buffer to receive hkdf value.
  @param[in]   out_size          size of hkdf bytes to generate.

  @retval TRUE   Hkdf generated successfully.
  @retval FALSE  Hkdf generation failed.

**/
boolean hkdf_sha256_expand(IN const uint8 *prk, IN uintn prk_size,
               IN const uint8 *info, IN uintn info_size,
               OUT uint8 *out, IN uintn out_size)
{
    try
    {
        CryptoPP::HKDF<CryptoPP::SHA256> hkdf;
        hkdf.DeriveKey(out, out_size, prk, prk_size, nullptr, 0,
                       info, info_size);
        return true;
    }
    catch (CryptoPP::Exception& e)
    {
        (void) e;
        return false;
    }
}

class EcCtx
{
public:
    EcCtx(CryptoPP::OID lwrve) : oid(lwrve), dh(lwrve),
        priv(dh.PrivateKeyLength()), pub(dh.PublicKeyLength()) {}

    CryptoPP::OID oid;
    CryptoPP::ECDH<CryptoPP::ECP>::Domain dh;
    CryptoPP::SecByteBlock priv, pub;
};

/**
  Allocates and Initializes one Elliptic Lwrve context for subsequent use
  with the NID.

  @param nid cipher NID

  @return  Pointer to the Elliptic Lwrve context that has been initialized.
           If the allocations fails, ec_new_by_nid() returns NULL.

**/
void *ec_new_by_nid(IN uintn nid)
{
    try
    {
        switch (nid)
        {
            case CRYPTO_NID_SECP256R1:
                return new EcCtx(CryptoPP::ASN1::secp256r1());
            case CRYPTO_NID_SECP384R1:
                return new EcCtx(CryptoPP::ASN1::secp384r1());
            case CRYPTO_NID_SECP521R1:
                return new EcCtx(CryptoPP::ASN1::secp521r1());
            default:
                return NULL;
        }
    }
    catch (CryptoPP::Exception& e)
    {
        (void) e;
        return NULL;
    }
}

/**
  Release the specified EC context.

  @param[in]  ec_context  Pointer to the EC context to be released.

**/
void ec_free(IN void *ec_context)
{
    try
    {
        delete reinterpret_cast<EcCtx *>(ec_context);
    }
    catch (CryptoPP::Exception& e)
    {
        (void) e;
        return;
    }
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
    try
    {
        EcCtx *ctx = reinterpret_cast<EcCtx *>(ec_context);

        if ((ec_context == NULL) || (public_key == NULL))
        {
            return false;
        }

        if (public_key_size != (ctx->pub.SizeInBytes() - 1))
        {
            return false;
        }

        // CryptoPP public key expects 0x04 prefix, we will prepend to key.
        ctx->pub.BytePtr()[0] = 0x4;
        memcpy(ctx->pub.BytePtr() + 1, public_key, public_key_size);
        return true;
    }
    catch (CryptoPP::Exception& e)
    {
        (void) e;
        return false;
    }
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
    try
    {
        EcCtx *ctx = reinterpret_cast<EcCtx *>(ec_context);

        if ((ec_context == NULL) || (public_key == NULL))
        {
            return false;
        }

        if ((public_key_size == NULL) || (*public_key_size < (ctx->pub.SizeInBytes() - 1)))
        {
            return false;
        }

        // We ignore first byte which is 0x04 prefix.
        memcpy(public_key, ctx->pub.BytePtr() + 1, ctx->pub.SizeInBytes() - 1);
        *public_key_size = ctx->pub.SizeInBytes() - 1;

        return true;
    }
    catch (CryptoPP::Exception& e)
    {
        (void) e;
        return false;
    }
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

  If ec_context is NULL, then return FALSE.
  If public_size is NULL, then return FALSE.
  If public_size is large enough but public is NULL, then return FALSE.

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
    try
    {
        EcCtx *ctx = reinterpret_cast<EcCtx *>(ec_context);
        CryptoPP::AutoSeededRandomPool rng;

        if ((ec_context == NULL) || (public_key == NULL))
        {
            return false;
        }

        // Account for fact that CryptoPP requires 0x04 prefix for public key.
        if ((public_key_size == NULL) || (*public_key_size < (ctx->pub.SizeInBytes() - 1)))
        {
            return false;
        }

        ctx->dh.GenerateKeyPair(rng, ctx->priv, ctx->pub);

        memcpy(public_key, ctx->pub.BytePtr() + 1, ctx->pub.SizeInBytes() - 1);
        *public_key_size = ctx->pub.SizeInBytes() - 1;

        return true;
    }
    catch (CryptoPP::Exception& e)
    {
        (void) e;
        return false;
    }
}

/**
  Computes exchanged common key.

  Given peer's public key (X, Y), this function computes the exchanged common key,
  based on its own context including value of lwrve parameter and random secret.
  X is the first half of peer_public with size being peer_public_size / 2,
  Y is the second half of peer_public with size being peer_public_size / 2.

  If ec_context is NULL, then return FALSE.
  If peer_public is NULL, then return FALSE.
  If peer_public_size is 0, then return FALSE.
  If key is NULL, then return FALSE.
  If key_size is not large enough, then return FALSE.

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
    try
    {
        if ((ec_context == NULL) || (peer_public == NULL) ||
            (key_size == NULL) || (key == NULL))
        {
            return false;
        }

        EcCtx *ctx = reinterpret_cast<EcCtx *>(ec_context);
        CryptoPP::SecByteBlock shared(ctx->dh.AgreedValueLength());
        CryptoPP::SecByteBlock peerExpanded(ctx->dh.PublicKeyLength());

        //
        // Peer public key won't have 0x04 prefix that CryptoPP requires.
        // Account for it in size, then add it before performing secret agreement.
        //
        if ((peer_public_size != (ctx->dh.PublicKeyLength() - 1)) ||
            (*key_size < ctx->dh.AgreedValueLength()))
        {
            return false;
        }

        peerExpanded.BytePtr()[0] = 0x04;
        memcpy(peerExpanded.BytePtr() + 1, peer_public, ctx->dh.PublicKeyLength() - 1);

        if(!ctx->dh.Agree(shared, ctx->priv, peerExpanded))
        {
            return false;
        }

        memcpy(key, shared.BytePtr(), shared.SizeInBytes());
        *key_size = shared.SizeInBytes();

        return true;
    }
    catch (CryptoPP::Exception& e)
    {
        (void) e;
        return false;
    }
}

/**
  Carries out the EC-DSA signature.

  This function carries out the EC-DSA signature.
  If the signature buffer is too small to hold the contents of signature, FALSE
  is returned and sig_size is set to the required buffer size to obtain the signature.

  If ec_context is NULL, then return FALSE.
  If message_hash is NULL, then return FALSE.
  If hash_size need match the hash_nid. hash_nid could be SHA256, SHA384, SHA512, SHA3_256, SHA3_384, SHA3_512.
  If sig_size is large enough but signature is NULL, then return FALSE.

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
    try
    {
        EcCtx *ctx = reinterpret_cast<EcCtx *>(ec_context);
        CryptoPP::AutoSeededRandomPool prng;

        if ((ec_context == NULL) || (message_hash == NULL) ||
            (signature == NULL) || (sig_size == NULL))
        {
            return false;
        }

        // Validate hash size. Signature size is validated below.
        switch (hash_nid)
        {
            // We only support SHA256 for now.
            case CRYPTO_NID_SHA256:
            {
                if (hash_size != SHA256_DIGEST_SIZE)
                {
                    return false;
                }
                break;
            }
            case CRYPTO_NID_SHA384:
            case CRYPTO_NID_SHA512:
            default:
            {
                return false;
            }
        }

        CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PrivateKey privateKey;
        privateKey.Initialize(ctx->oid, CryptoPP::Integer(ctx->priv.begin(), ctx->priv.size()));
        CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::Signer signer(privateKey);
        if (*sig_size < signer.SignatureLength())
        {
            return false;
        }        
        *sig_size = signer.SignMessage(prng, message_hash, hash_size, signature);

        return true;
    }
    catch (CryptoPP::Exception& e)
    {
        (void) e;
        return false;
    }
}

/**
  Verifies the EC-DSA signature.

  If ec_context is NULL, then return FALSE.
  If message_hash is NULL, then return FALSE.
  If signature is NULL, then return FALSE.
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
    try
    {
        EcCtx *ctx = reinterpret_cast<EcCtx *>(ec_context);
        CryptoPP::AutoSeededRandomPool prng;
        uint8 hashRev[32];

        if ((ec_context == NULL) || (message_hash == NULL) || (signature == NULL))
        {
            return false;
        }

        // Validate hash size. Signature size is validated below.
        switch (hash_nid)
        {
            // We only support SHA256 for now.
            case CRYPTO_NID_SHA256:
            {
                if (hash_size != SHA256_DIGEST_SIZE)
                {
                    return false;
                }
                break;
            }
            case CRYPTO_NID_SHA384:
            case CRYPTO_NID_SHA512:
            default:
            {
                return false;
            }
        }

        int hSize = ctx->pub.size() / 2;
        CryptoPP::ECP::Point point(CryptoPP::Integer(ctx->pub.BytePtr() + 1, hSize),
                        CryptoPP::Integer(ctx->pub.BytePtr() + 1 + hSize, hSize));

        CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey publicKey;
        publicKey.Initialize(ctx->oid, point);

        CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::Verifier verifier(publicKey);
        if (sig_size < verifier.SignatureLength())
        {
            return false;
        }
        return verifier.VerifyMessage(hashRev, hash_size, signature, sig_size);
    }
    catch (CryptoPP::Exception& e)
    {
        (void) e;
        return false;
    }
}

/**
  Generates a pseudorandom byte stream of the specified size.

  If output is NULL, then return FALSE.
  If this interface is not supported, then return FALSE.

  @param[out]  output  Pointer to buffer to receive random value.
  @param[in]   size    size of random bytes to generate.

  @retval TRUE   Pseudorandom byte stream generated successfully.
  @retval FALSE  Pseudorandom number generator fails to generate due to lack of entropy.
  @retval FALSE  This interface is not supported.

**/
boolean random_bytes(OUT uint8 *output, IN uintn size)
{
    try
    {
        CryptoPP::AutoSeededRandomPool prng;
        prng.GenerateBlock(output, size);
        return true;
    }
    catch (CryptoPP::Exception& e)
    {
        (void) e;
        return false;
    }
}

/**
  Sets up the seed value for the pseudorandom number generator.

  This function sets up the seed value for the pseudorandom number generator.
  If seed is not NULL, then the seed passed in is used.
  If seed is NULL, then default seed is used.
  If this interface is not supported, then return FALSE.

  @param[in]  seed      Pointer to seed value.
                        If NULL, default seed is used.
  @param[in]  seed_size  size of seed value.
                        If seed is NULL, this parameter is ignored.

  @retval TRUE   Pseudorandom number generator has enough entropy for random generation.
  @retval FALSE  Pseudorandom number generator does not have enough entropy for random generation.
  @retval FALSE  This interface is not supported.

**/
boolean random_seed(IN const uint8 *seed OPTIONAL, IN uintn seed_size)
{
    // Since for testing, we won't bother to seed RNG.
    return TRUE;
}
