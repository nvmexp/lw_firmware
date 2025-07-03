/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */
/**
 * @file   crypto_ops_ccm.h
 * @author Juki <jvirtanen@nvidia.com>
 * @date   2019
 *
 * Definitions for common crypto AES-CCM mode.
 * These are used by CCC internally.
 */
#ifndef INCLUDED_CRYPTO_OPS_CCM_H
#define INCLUDED_CRYPTO_OPS_CCM_H

#include <crypto_ops.h>

#if HAVE_AES_CCM

#define AAD_LEN_TWOBYTE_LIMIT 65280UL

#if !defined(CCC_AES_CCM_WORK_BUFFER_SIZE)
/* DMA aligned contiguous buffer */
#define CCC_AES_CCM_WORK_BUFFER_SIZE 512U
#endif

#define CCM_NONCE_MIN_LEN 7U
#define CCM_NONCE_MAX_LEN 13U

#define CCM_TAG_MIN_LEN 4U
#define CCM_TAG_MAX_LEN SE_AES_BLOCK_LENGTH

/* @def AES_CCM_TAG_LEN_VALID(tag_len)
 *
 * @param tag_blen byte length of AES-CCM tag
 * @return true if tag_len is valid
 */
#define AES_CCM_TAG_LEN_VALID(tag_blen)	    \
	((((tag_blen) & 0x01U) == 0U) &&    \
	 ((tag_blen) >= CCM_TAG_MIN_LEN) && \
	 ((tag_blen) <= CCM_TAG_MAX_LEN))

/* @def AES_CCM_NONCE_LEN_VALID(nonce_len)
 *
 * @param nonce_blen byte length of AES-CCM nonce
 * @return true if nonce_len is valid
 */
#define AES_CCM_NONCE_LEN_VALID(nonce_blen)				\
	(((nonce_blen) >= CCM_NONCE_MIN_LEN) &&				\
	 ((nonce_blen) <= CCM_NONCE_MAX_LEN))

/**
 * @brief process layer handler for AES-CCM related operations.
 *
 * This function is called by (e.g.) the CLASS handler for AE.
 * In this case AES-CCM mode to process data passed in the TE_OP_DOFINAL operation.
 * AES-CCM does not support TE_OP_UPDATE calls, it is always a single
 * shot operation completing on the first call.
 *
 * AES-CCM is a two pass algorithm: It scans the input data
 *  once for calculating CBC-MAC tag value for AAD+PAYLOAD
 *  and once for AES-CTR mode operation to handle payload.
 *
 * AES-CCM cipher  : CBC-MAC data first, then AES-CTR cipher it.
 *                   Cipher text length grows by encrypted TAG value.
 *
 * AES-CCM decipher: AES-CTR decipher data+mac, then calculate CBC-MAC
 *                   and verify MAC. Only INVALID answer return permitted
 *                   on MAC verifcation failures.
 *
 * se_aes_ccm_process_input() uses the following fields from input_params:
 * - src        : address of data to process
 * - input_size : number of data bytes to process
 * - dst        : result output buffer
 * - output_size: size of dst buffer [IN], number of result bytes written [OUT]
 *
 * @param input_params input/output object with buffer addresses and sizes.
 * @param aes_context AES context
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t se_aes_ccm_process_input(
	struct se_data_params *input_params,
	struct se_aes_context *aes_context);

/* For backwards compatibility until callers in
 * other subsystems handled (mainly for GVS)
 *
 * Make the old function name call the new function.
 * Remove this after transition complete.
 */
#define se_ccm_process_input(in, aes_ctx) se_aes_ccm_process_input(in, aes_ctx)

#endif /* HAVE_AES_CCM */
#endif /* INCLUDED_CRYPTO_OPS_CCM_H */
