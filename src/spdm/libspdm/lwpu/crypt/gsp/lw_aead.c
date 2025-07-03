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
 * @file   lw_aead.c
 * @brief  File that provides AEAD functionality from SEC2 to libSPDM.
 */

/* ------------------------- System Includes -------------------------------- */
#include "base.h"
#include "library/cryptlib.h"

#include "tegra_se.h"
#include "tegra_cdev.h"
#include <liblwriscv/print.h>
#include <liblwriscv/libc.h>
#include <limits.h>

#define CTR_START_PROCESS_BIGENDIAN         ((LwU32)0x01000000) 
#define GCM_IV_SIZE_BYTES                   ((LwU32)(12))
#define GCM_MIN_TAG_SIZE_BYTES              ((LwU32)(12))
#define GCM_MAX_TAG_SIZE_BYTES              ((LwU32)(16))

/* ------------------------- Private Functions ------------------------------- */
static status_t GCC_ATTRIB_SECTION("imem_libspdm", "libcccGcmProcessBlock")
libcccGcmProcessBlock(struct se_engine_aes_context* econtext, const LwU8* srcBuf,
    LwU8* dstBuf, LwU32 srcLen, LwU32 dstLen);

static status_t GCC_ATTRIB_SECTION("imem_libspdm", "libcccGcmCompute")
libcccGcmCompute(const LwU8*  inputBuf, LwU32  inputBufLen, LwU8*  outputBuf,
    LwU32* outputBufLen, void*  tagOutput, LwU32  tag_size, const LwU8*  key,
    LwU32  key_size, const LwU8*  iv, LwU32  iv_size, const LwU8*  addData,
    LwU32  addLen, LwBool isEncrypt);
/*
 *  @brief AES GCM CCC Lite API
 *
 *  @param econtext: engine context
 *  @param input_params: pointer to struct which needs to reference the I/O buffers
 *  @param srcBuf: Input buffer (NULL for tag generation)
 *  @param dstBuf: Output buffer
 *  @param srcLen: length of input buffer (0 for tag generation)
 *  @param dstLen: length of output buffer
 *
 *  @note The IV is incremented just before the block is processed (unless econtext->is_first=true in which case the IV
 *  increment is skipped). So when engine_aes_process_block_locked returns, the engine context contains the IV which was
 *  just used to process the block.
 *
 *  @return NO_ERROR on success, error (from error_code.h) on failure
 */
static status_t 
libcccGcmProcessBlock(
    struct se_engine_aes_context* econtext,
    const LwU8* srcBuf,
    LwU8* dstBuf,
    LwU32 srcLen,
    LwU32 dstLen
)
{
    struct se_data_params input_params;
    input_params.src         = srcBuf;
    input_params.dst         = dstBuf;
    input_params.input_size  = srcLen;
    input_params.output_size = dstLen;

    return engine_aes_process_block_locked(&input_params, econtext);
}

/*
 *  @brief Performs AES GCM Encryption/Decryption
 *
 *  @param inputBuf:    input data which should be encrypted/decrypted
 *  @param inputBufLen: length of input/output buffers (these should be same length)
 *  @param outputBuf:   buffer to write encrypted/decrypted text to
 *  @param outputBufLen:length of output buffers (these should be same length)
 *  @param tagOutput:   buffer to write GCM tag to
 *  @param key:         AES256 encryption key
 *  @param key:         AES256 encryption key size
 *  @param iv:          buffer containing random iv (96 bit)
 *  @param iv_size:     random iv size
 *  @param addData:     Pointer to the additional authenticated data (AAD).
 *  @param isEncrypt:   Whether its AES-GCM Encryption or Decryption
 *
 *  @return NO_ERROR on success, error (from error_code.h) on failure
 */
static status_t
libcccGcmCompute(
    const LwU8*  inputBuf,
          LwU32  inputBufLen,
          LwU8*  outputBuf,
          LwU32* outputBufLen,
          void*  tagOutput,
          LwU32  tag_size,
    const LwU8*  key,
          LwU32  key_size,
    const LwU8*  iv,
          LwU32  iv_size,
    const LwU8*  addData,
          LwU32  addLen,
          LwBool isEncrypt
)
{
    engine_t *engine                           = NULL;
    struct se_engine_aes_context econtext      = {0}; // se_engine_aes_context_t
    LwU8 callwlatedTag[GCM_MAX_TAG_SIZE_BYTES] = {0};
    LwU32 gcmCounterBigEndian[4]               = {0};
    status_t res                               = NO_ERROR;

    if (inputBuf == NULL || outputBuf == NULL || inputBufLen == 0 || outputBufLen == NULL
        || tagOutput == NULL || key == NULL || iv == NULL)
    {
        return FALSE;
    }
    if (inputBufLen > INT_MAX || addLen > INT_MAX || iv_size != GCM_IV_SIZE_BYTES) 
    {
        return FALSE;
    }
    switch (key_size) 
    {
        case 16:
        case 24:
        case 32:
            break;
        default:
            return FALSE;
    }
    if((tag_size < GCM_MIN_TAG_SIZE_BYTES) || (tag_size > GCM_MAX_TAG_SIZE_BYTES))
    {
        return FALSE;
    }
    if (outputBufLen != NULL) 
    {
        if ((*outputBufLen > INT_MAX) || (*outputBufLen < inputBufLen)) 
        {
            return FALSE;
        }
    }

    res = ccc_select_engine((const struct engine_s **)&engine, CCC_ENGINE_CLASS_AES, ENGINE_SE0_AES1);
    if (res != NO_ERROR)
    {
#ifndef GSP_CC
        printf("Error while selecting SE0 AES1 engine\n");
#endif // GSP_CC
        return FALSE;
    }

    memcpy((void*) gcmCounterBigEndian, (void*) iv, GCM_IV_SIZE_BYTES);
    gcmCounterBigEndian[3]        = CTR_START_PROCESS_BIGENDIAN;
    econtext.engine               = engine;
    econtext.cmem                 = NULL;
    econtext.is_encrypt           = isEncrypt;
    econtext.is_hash              = false; 
    econtext.is_first             = true;
    econtext.is_last              = false;
    econtext.se_ks.ks_keydata     = key;
    econtext.se_ks.ks_bitsize     = key_size << 3; // Colwerting key_size from bytes to bits 
    econtext.se_ks.ks_subkey_slot = 0;
    econtext.aes_flags            = (AES_FLAGS_DYNAMIC_KEYSLOT | AES_FLAGS_CTR_MODE_BIG_ENDIAN | AES_FLAGS_HW_CONTEXT);
    econtext.algorithm            = TE_ALG_AES_GCM;
    econtext.gcm.counter          = gcmCounterBigEndian;
    if(isEncrypt)
    {
        econtext.gcm.tag     = (LwU8*)tagOutput;
    }
    else
    {
        econtext.gcm.tag     = callwlatedTag;
    }

    if((addLen != 0) && (addData != NULL))
    {
        econtext.aes_flags |= AES_FLAGS_AE_AAD_DATA;
        res = libcccGcmProcessBlock(&econtext, addData, NULL, addLen, 0);
        if((res == NO_ERROR) && (econtext.gcm.aad_len != addLen))
        {
            res = ERR_GENERIC;
        }
        CCC_ERROR_CHECK(res);
    }

    res = libcccGcmProcessBlock(&econtext, inputBuf, outputBuf, inputBufLen, inputBufLen);
    CCC_ERROR_CHECK(res);

    econtext.aes_flags |= AES_FLAGS_AE_FINALIZE_TAG;

    if(!isEncrypt)
    {
        econtext.aes_flags |= AES_FLAGS_HW_CONTEXT;
    }
    econtext.is_first  = false;
    econtext.is_last   = true;

    res = libcccGcmProcessBlock(&econtext, NULL,  econtext.gcm.tag, inputBufLen, tag_size);
    CCC_ERROR_CHECK(res);

    if(!isEncrypt)
    {
        if(memcmp(callwlatedTag, (LwU8*)tagOutput, tag_size) != 0)
        {
            res = ERR_GENERIC;
        }
    }
    *outputBufLen = inputBufLen;
fail:
    return res;
}

/* ------------------------- Public Functions ------------------------------- */
/*
Performs AEAD AES-GCM authenticated encryption on a data buffer and additional authenticated data (AAD).

  iv_size must be 12, otherwise FALSE is returned.
  key_size must be 16, 24 or 32, otherwise FALSE is returned.
  tag_size must be 12, 13, 14, 15, 16, otherwise FALSE is returned.

  @param[in]   key           Pointer to the encryption key.
  @param[in]   key_size      size of the encryption key in bytes.
  @param[in]   iv            Pointer to the IV value.
  @param[in]   iv_size       size of the IV value in bytes.
  @param[in]   a_data        Pointer to the additional authenticated data (AAD).
  @param[in]   a_data_size   size of the additional authenticated data (AAD) in bytes.
  @param[in]   data_in       Pointer to the input data buffer to be encrypted.
  @param[in]   data_in_size  size of the input data buffer in bytes.
  @param[out]  tag_out       Pointer to a buffer that receives the authentication tag output.
  @param[in]   tag_size      size of the authentication tag in bytes.
  @param[out]  data_out      Pointer to a buffer that receives the encryption output.
  @param[out]  data_out_size size of the output data buffer in bytes.

  @retval TRUE   AEAD AES-GCM authenticated encryption succeeded.
  @retval FALSE  AEAD AES-GCM authenticated encryption failed.
*/
boolean aead_aes_gcm_encrypt(IN const uint8 *key, IN uintn key_size,
                 IN const uint8 *iv, IN uintn iv_size,
                 IN const uint8 *a_data, IN uintn a_data_size,
                 IN const uint8 *data_in, IN uintn data_in_size,
                 OUT uint8 *tag_out, IN uintn tag_size,
                 OUT uint8 *data_out, OUT uintn *data_out_size)
{

    status_t res = NO_ERROR;

    res = libcccGcmCompute(data_in, data_in_size, data_out, (LwU32*)data_out_size, (void*)tag_out, tag_size, key, key_size, iv, iv_size, a_data, a_data_size, LW_TRUE);
    if (res != NO_ERROR)
    {
        return FALSE;
    }
    return TRUE;
}

/*
  Performs AEAD AES-GCM authenticated decryption on a data buffer and additional authenticated data (AAD).

  iv_size must be 12, otherwise FALSE is returned.
  key_size must be 16, 24 or 32, otherwise FALSE is returned.
  tag_size must be 12, 13, 14, 15, 16, otherwise FALSE is returned.
  If additional authenticated data verification fails, FALSE is returned.

  @param[in]   key           Pointer to the encryption key.
  @param[in]   key_size      size of the encryption key in bytes.
  @param[in]   iv            Pointer to the IV value.
  @param[in]   iv_size       size of the IV value in bytes.
  @param[in]   a_data        Pointer to the additional authenticated data (AAD).
  @param[in]   a_data_size   size of the additional authenticated data (AAD) in bytes.
  @param[in]   data_in       Pointer to the input data buffer to be decrypted.
  @param[in]   data_in_size  size of the input data buffer in bytes.
  @param[in]   tag           Pointer to a buffer that contains the authentication tag.
  @param[in]   tag_size      size of the authentication tag in bytes.
  @param[out]  data_out      Pointer to a buffer that receives the decryption output.
  @param[out]  data_out_size size of the output data buffer in bytes.

  @retval TRUE   AEAD AES-GCM authenticated decryption succeeded.
  @retval FALSE  AEAD AES-GCM authenticated decryption failed.
*/
boolean aead_aes_gcm_decrypt(IN const uint8 *key, IN uintn key_size,
                 IN const uint8 *iv, IN uintn iv_size,
                 IN const uint8 *a_data, IN uintn a_data_size,
                 IN const uint8 *data_in, IN uintn data_in_size,
                 IN const uint8 *tag, IN uintn tag_size,
                 OUT uint8 *data_out, OUT uintn *data_out_size)
{
    status_t res = NO_ERROR;

    res = libcccGcmCompute(data_in, data_in_size, data_out, (LwU32*)data_out_size, (void*)tag, tag_size, key, key_size, iv, iv_size, a_data, a_data_size, LW_FALSE);
    if (res != NO_ERROR)
    {
        return FALSE;
    }
    return TRUE;
}
