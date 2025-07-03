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
 * @file   lw_aead.c
 * @brief  File that provides AEAD functionality from SEC2 to libspdm.
 */

/* ------------------------- System Includes ------------------------------- */
#include "base.h"
#include "lwuproc.h"

/* ------------------------- Application Includes -------------------------- */
#include "lw_crypt.h"
#include "library/cryptlib.h"
#include "spdm_lib_config.h"
#include "lib_intfcshahw.h"
#include "scp_internals.h"

// APM-TODO CONFCOMP-400: Replace with HS version of SCP library.
#include "apm/sec2_apmscpcrypt.h"

/* ------------------------- Prototypes ------------------------------------- */
static boolean _aead_aes_128_ctr_hmac_sha_256(IN boolean encrypt_data,
    IN const uint8 *key, IN uintn key_size, IN const uint8 *iv, IN uintn iv_size,
    IN const uint8 *a_data, IN uintn a_data_size, IN const uint8 *data_in,
    IN uintn data_in_size, IN OUT uint8 *tag, IN uintn tag_size,
    OUT uint8 *data_out, OUT uintn *data_out_size)
    GCC_ATTRIB_SECTION("imem_spdm", "_aead_aes_128_ctr_hmac_sha_256");

/* ------------------------- Private Functions ------------------------------ */
/**
  Helper function to perform AEAD via AES-128 CTR and HMAC-SHA256. The tag is
  callwlated by the concatenation of the additional authenticated data (AAD),
  and the encrypted data buffer, or (AAD || encrypted data). The same key is
  used for AES encryption and for HMAC.

  Works as AES-128-CTR does not change for encryption or decryption - rather,
  depending on the operation specified, the HMAC tag is callwlated before or
  after the crypt operation. On decryption, it compares the provided tag with
  the callwlated tag, failing if they do not match.

  At least one of the data buffer or AAD must be specified in order to
  perform AEAD. If only the data buffer is specified, the tag is callwlated
  without AAD. If only AAD is specified, the tag is callwlated with only AAD,
  and no decryption is performed.

  iv_size must be 12, otherwise FALSE is returned.
  key_size must be 16, otherwise FALSE is returned.
  tag_size must be 32, otherwise FALSE is returned.
  If additional authenticated data verification fails, FALSE is returned.

  @param[in]      encrypt_data   Whether AEAD encryption should be performed.
                                 If false, AEAD decryption is performed.
  @param[in]      key            Pointer to the encryption key.
  @param[in]      key_size       Size of the encryption key in bytes.
  @param[in]      iv             Pointer to the IV value.
  @param[in]      iv_size        Size of the IV value in bytes.
  @param[in]      a_data         Pointer to the additional authenticated data (AAD).
  @param[in]      a_data_size    Size of the additional authenticated data (AAD) in bytes.
  @param[in]      data_in        Pointer to the input data buffer to be decrypted.
  @param[in]      data_in_size   Size of the input data buffer in bytes.
  @param[in, out] tag            Pointer to a buffer that contains the authentication tag.
                                 If encryption, the buffer will contain output tag.
                                 If decryption, the buffer contains expected tag.
  @param[in]      tag_size       Size of the authentication tag in bytes.
  @param[out]     data_out       Pointer to a buffer that receives the decryption output.
  @param[out]     data_out_size  Size of the output data buffer in bytes.

  @retval TRUE   AEAD AES-128-CTR & HMAC-SHA256 AEAD succeeded.
  @retval FALSE  AEAD AES-128-CTR & HMAC-SHA256 AEAD failed.

**/
static boolean
_aead_aes_128_ctr_hmac_sha_256
(
    IN     boolean      encrypt_data,
    IN     const uint8 *key,
    IN     uintn        key_size,
    IN     const uint8 *iv,
    IN     uintn        iv_size,
    IN     const uint8 *a_data,
    IN     uintn        a_data_size,
    IN     const uint8 *data_in,
    IN     uintn        data_in_size,
    IN OUT uint8       *tag,
    IN     uintn        tag_size,
    OUT    uint8       *data_out,
    OUT    uintn       *data_out_size
)
{
    LwU8  *dataAligned;
    LwU8  *keyAligned;
    LwU8  *ivAligned;
    LwU32  dataSizePadded;
    LwU8   dataBuf[MAX_SPDM_MESSAGE_BUFFER_SIZE + SCP_BUF_ALIGNMENT];
    LwU8   keyBuf[SCP_AES_SIZE_IN_BYTES + SCP_BUF_ALIGNMENT];
    LwU8   ivBuf[SCP_AES_SIZE_IN_BYTES + SCP_BUF_ALIGNMENT];
    LwU8   tagBuf[SHA_256_HASH_SIZE_BYTE];

    // Basic parameter validation, ensure at least one source of data is included.
    if (key == NULL || iv == NULL || tag == NULL ||
        (a_data == NULL && data_in == NULL))
    {
        return LW_FALSE;
    }

    // Basic size validation.
    if (key_size != SCP_AES_SIZE_IN_BYTES        ||
        iv_size  != AES_128_CTR_IV_SIZE_IN_BYTES ||
        tag_size != SHA_256_HASH_SIZE_BYTE)
    {
        return LW_FALSE;
    }

    // Validate authenticated data, if provided.
    if (a_data != NULL && a_data_size == 0)
    {
        return LW_FALSE;
    }

    // Validate data to encrypt/decrypt, if provided.
    if (data_in != NULL)
    {
        // Ensure input and output are provided, and are of relevant size.
        if (data_out == NULL || data_out_size == NULL || data_in_size == 0 ||
            data_in_size > MAX_SPDM_MESSAGE_BUFFER_SIZE ||
            data_in_size > *data_out_size)
        {
            return LW_FALSE;
        }
    }

    // Validate that we have space to callwlate tag, checking for underflow.
    if ((a_data_size + data_in_size) > MAX_SPDM_MESSAGE_BUFFER_SIZE ||
        (a_data_size + data_in_size) < a_data_size ||
        (a_data_size + data_in_size) < data_in_size)
    {
        return LW_FALSE;
    }

    zero_mem(dataBuf, sizeof(dataBuf));
    zero_mem(keyBuf,  sizeof(keyBuf));
    zero_mem(ivBuf,   sizeof(ivBuf));
    zero_mem(tagBuf,  sizeof(tagBuf));

    // If decrypting data, callwlate HMAC tag and validate before decrypting.
    if (!encrypt_data)
    {
        //
        // Copy all data to dataBuf. We consider HMAC message
        // as (AAD || encrypted data).
        //
        if (a_data != NULL)
        {
            copy_mem(dataBuf, a_data, a_data_size);
        }

        if (data_in != NULL)
        {
            copy_mem(dataBuf + a_data_size, data_in, data_in_size);
        }

        // Callwlate HMAC of encrypted buffer and a_data to get tag.
        if (!hmac_sha256_all(dataBuf, a_data_size + data_in_size,
                             key, key_size, tagBuf))
        {
            return LW_FALSE;
        }

        // Compare tags to ensure integrity, failing if does not match.
        if (compare_mem(tagBuf, tag, tag_size) != 0)
        {
            return LW_FALSE;
        }
    }

    //
    // Perform encryption/decryption on any provided data.
    // AES-CTR operation is equivalent for encryption and decryption.
    //
    if (data_in != NULL)
    {
        //
        // Align buffers to ensure SCP compatibility, then copy data to buffers.
        // Assert required for correctness of LW_ALIGN_UP operations.
        //
        ct_assert(SCP_BUF_ALIGNMENT <= SCP_AES_SIZE_IN_BYTES);
        dataAligned = (uint8 *)LW_ALIGN_UP((LwUPtr)dataBuf, SCP_BUF_ALIGNMENT);
        keyAligned  = (uint8 *)LW_ALIGN_UP((LwUPtr)keyBuf,  SCP_BUF_ALIGNMENT);
        ivAligned   = (uint8 *)LW_ALIGN_UP((LwUPtr)ivBuf,   SCP_BUF_ALIGNMENT);

        copy_mem(dataAligned, data_in, data_in_size);
        copy_mem(keyAligned,  key,     key_size);

        //
        // The total IV buffer that SCP uses is size SCP_AES_SIZE_IN_BYTES. To reach
        // this size, the input IV is zero-extended at the end, and the zeroed bytes
        // act as the block counter. We can just extend the size, since we know IV
        // buffer must be big enough, and the unused bytes have already been zeroed.
        //
        copy_mem(ivAligned, iv, iv_size);

        //
        // Add padding to msg_buf if necessary. SCP does not support message sizes
        // that are not block size aligned, however, AES-CTR does for the final
        // block. Therefore, we pad the final block and only copy out the original
        // message size. To support this, msg_buf is already padded accordingly.
        //
        dataSizePadded = data_in_size;
        if ((dataSizePadded % SCP_AES_SIZE_IN_BYTES) != 0)
        {
            LwU32 paddingNeeded = (SCP_AES_SIZE_IN_BYTES) -
                                  (dataSizePadded % SCP_AES_SIZE_IN_BYTES);
            dataSizePadded += paddingNeeded;
        }

        //
        // Perform AES-128-CTR encryption.
        // APM-TODO CONFCOMP-400: Replace with HS version of SCP library.
        //
        if (apmScpCtrCryptWithKey(keyAligned, LW_FALSE, dataAligned,
                                  dataSizePadded, dataAligned,
                                  ivAligned, LW_FALSE) != FLCN_OK)
        {
            return LW_FALSE;
        }

        // Copy result to output buffer.
        copy_mem(data_out, dataAligned, data_in_size);
        *data_out_size = data_in_size;
    }

    // If encrypting, callwate tag now that data is encrypted.
    if (encrypt_data)
    {
        //
        // Copy all data to dataBuf. We consider HMAC message
        // as (AAD || encrypted data).
        //
        if (a_data != NULL)
        {
            copy_mem(dataBuf, a_data, a_data_size);
        }

        if (data_out != NULL)
        {
            copy_mem(dataBuf + a_data_size, data_out, data_in_size);
        }

        // Get HMAC of AAD and encrypted data, and copy tag to output buffer.
        if (!hmac_sha256_all(dataBuf, a_data_size + data_in_size,
                             key, key_size, tag))
        {
            return LW_FALSE;
        }
    }

    return LW_TRUE;
}

/* ------------------------- Lw_crypt.h Functions --------------------------- */
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
  @param[in]  a_data_size    size of the additional authenticated data (AAD) in bytes.
  @param[in]  data_in        Pointer to the input data buffer to be encrypted.
  @param[in]  data_in_size   Size of the input data buffer in bytes.
  @param[out] tag_out        Pointer to a buffer that receives the authentication tag output.
  @param[in]  tag_size       Size of the authentication tag in bytes.
  @param[out] data_out       Pointer to a buffer that receives the encryption output.
  @param[out] data_out_size  Size of the output data buffer in bytes.

  @retval TRUE   AEAD AES-128-CTR & HMAC-SHA256 authenticated encryption succeeded.
  @retval FALSE  AEAD AES-128-CTR & HMAC-SHA256 authenticated encryption failed.

**/
boolean
aead_aes_ctr_hmac_sha_encrypt
(
    IN  const uint8 *key,
    IN  uintn        key_size,
    IN  const uint8 *iv,
    IN  uintn        iv_size,
    IN  const uint8 *a_data,
    IN  uintn        a_data_size,
    IN  const uint8 *data_in,
    IN  uintn        data_in_size,
    OUT uint8       *tag_out,
    IN  uintn        tag_size,
    OUT uint8       *data_out,
    OUT uintn       *data_out_size
)
{
    return _aead_aes_128_ctr_hmac_sha_256(LW_TRUE, key, key_size, iv, iv_size,
                                          a_data, a_data_size, data_in,
                                          data_in_size, tag_out, tag_size,
                                          data_out, data_out_size);
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

  @retval TRUE   AEAD AES-128-CTR & HMAC-SHA256 authenticated decryption succeeded.
  @retval FALSE  AEAD AES-128-CTR & HMAC-SHA256 authenticated decryption failed.

**/
boolean
aead_aes_ctr_hmac_sha_decrypt
(
    IN  const uint8 *key,
    IN  uintn        key_size,
    IN  const uint8 *iv,
    IN  uintn        iv_size,
    IN  const uint8 *a_data,
    IN  uintn        a_data_size,
    IN  const uint8 *data_in,
    IN  uintn        data_in_size,
    IN  const uint8 *tag,
    IN  uintn        tag_size,
    OUT uint8       *data_out,
    OUT uintn       *data_out_size
)
{
    return _aead_aes_128_ctr_hmac_sha_256(LW_FALSE, key, key_size, iv, iv_size,
                                          a_data, a_data_size, data_in,
                                          data_in_size, (uint8 *)tag, tag_size,
                                          data_out, data_out_size);
}
