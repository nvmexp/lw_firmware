/*
 * Copyright (c) 2016-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */
/**
 * @file   crypto_ops_intern.h
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2016
 *
 * Definitions for common crypto code.
 * Context API and CLASS API objects and definitions.
 *
 * These are used by CCC internally.
 */
#ifndef INCLUDED_CRYPTO_OPS_INTERN_H
#define INCLUDED_CRYPTO_OPS_INTERN_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

#if CCC_SOC_WITH_PKA1
#include <tegra_pka1_gen.h>
#endif

#if CCC_SOC_WITH_LWPKA
#include <lwpka/tegra_lwpka_gen.h>
#endif

#define ASN1_TAG_SEQUENCE 0x30U
#define ASN1_TAG_INTEGER 0x02U
#define ASN1_INT_MAX_ONE_BYTE_VALUE 0x80U

/**
 *
 * @def TE_AES_BLOCK_CIPHER_MODE(algo)
 * @brief true if algorithm is AES block cipher mode.
 *
 * Block cipher modes always output block aligned data
 * and expect block aligned input (always for decipher and unpadding cipher;
 * for padding block cipher the data is padded to block boundary before
 * ciphering it).
 *
 * For stream ciphers the data is not ciphered; "something else" is ciphered and the
 * result is usually XORed to the data. This is only secure in case it is guaranteed
 * that the "something else" is UNIQUE for each and every AES block.
 * Otherwise => problems.
 */
#define TE_AES_BLOCK_CIPHER_MODE(algo)	m_aes_block_cipher_mode(algo)

static inline bool m_aes_block_cipher_mode(te_crypto_algo_t algo)
{
	return ((TE_ALG_AES_ECB == algo)       ||
		(TE_ALG_AES_CBC == algo)       ||
		(TE_ALG_AES_ECB_NOPAD == algo) ||
		(TE_ALG_AES_CBC_NOPAD == algo));
}

/** @brief Max byte size of the MAC algorithm result (CMAC/HMAC) */
#define TE_MAC_SIZE_MAX 64U

/** @def VALID_SHA_LEN(len)
 * @brief true if len is a valid SHA-1 or SHA-2 digest byte length
 */
static inline bool m_valid_sha_len(uint32_t len)
{
	return ((20U == len) || (28U == len) || (32U == len) ||
		(48U == len) || (64U == len));
}
#define VALID_SHA_LEN(len) m_valid_sha_len(len)

/**
 * @def VALID_DIGEST_LEN(len)
 * @brief true is len is valid known digest byte length
 *
 * - SHA-*: length checked with in VALID_SHA_LEN
 * - WHIRLPOOL: digest length == 64U (like SHA-512)
 * - MD5: digest length 16U
 */
#if HAVE_MD5
#define VALID_DIGEST_LEN(len) (VALID_SHA_LEN(len) || (16U == (len)))
#else
#define VALID_DIGEST_LEN(len) VALID_SHA_LEN(len)
#endif

struct crypto_context_s;

/**
 * @brief (Un)supported operation types for a given class.
 *
 * This object type is used for the context API CLASS handler
 * selection array const struct crypto_class_funs_s crypto_class_funs[].
 *
 * This array collects all class handler supported functions into a single
 * lookup array. Every supported algorithm is first mapped into
 * one of the classes in enum te_crypto_class_e and then the
 * table is scanned to check which primitive ops are supported for that class
 * primitive operations (TE_OP_INIT, etc).
 *
 * An error is generated if client attempts to use a
 * primitive operation that is not defined for the class.
 *
 * For the TE_OP_UPDATE and TE_OP_DOFINAL primitive operations
 * which can provide data to the crypto context the corresponding
 * class layer handlers will pass such data to the @ref process_layer_api
 *
 * The CLASS layer API functions are entered from the @ref context_layer_api
 *
 * @defgroup class_layer_api CLASS layer API
 */
/*@{*/
struct crypto_class_funs_s {
	te_crypto_class_t    cfun_class; /**< CLASS of the following handlers */
	uint32_t	     cfun_nosup; /**< CLASS functions not supported in this class */
};
/*@}*/

/**
 * @brief Get message digest result size for all known digest algos,
 * some of which may not be supported by config.
 *
 * @param algo selected crypto algorithm
 * @param len_p referenced object set to digest length in bytes on success
 *
 * @return NO_ERROR on success error code otherwise.
 */
status_t get_message_digest_size(te_crypto_algo_t algo, uint32_t *len_p);

#if HAVE_SE_SHA
/**
 * @brief Internal CCC helper function to callwlate SHA-1 or SHA-2 digest from a single
 * memory buffer.
 *
 * Digests the input_params->src buffer with given sha_algo. On success
 * input_params->output_size is set to SHA digest length and digest
 * written to input_params->dst. Error returned if digest
 * does not fit in dst.
 *
 * @param cmem Context memory or NULL to use subsystem memory pool
 * @param domain TE_CRYPTO_DOMAIN_TA for user space data addresses else TE_CRYPTO_DOMAIN_KERNEL.
 * @param input_params input/output object with buffer addresses and sizes
 * @param sha_algo TE_ALG_SHAxxx, one of the SHA-2 or SHA-1 algorithms
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t sha_digest(
	struct context_mem_s *cmem,
	te_crypto_domain_t domain,
	struct se_data_params *input_params,
	te_crypto_algo_t sha_algo);

/**
 * @brief Process layer functions for processing data passed to the crypto context
 * by the @ref class_layer_api
 *
 * These handlers deal with zero or more TE_OP_UPDATE operations (if
 * supported for the operation class) and exactly one TE_OP_DOFINAL
 * operation.
 *
 * When required the process layer handlers will use the @ref eng_switch_api
 * to process the data with the HW engine layer.
 *
 * @defgroup process_layer_api Process layer API
 */
/*@{*/
/* addgroup used to add members to this group below */
/*@}*/

/**
 * @brief process layer handler for all SHA engine related operations.
 *
 * This function is called by (e.g.) the CLASS handler for message digests
 * to process data passed in the TE_OP_UPDATE and TE_OP_DOFINAL operations
 * for SHA related message digests.
 *
 * There is only one HW engine dealing with message digests: HW SHA engine.
 * All other message digest class operations are implemented in software.
 *
 * When this code needs HW to do SHA digests, it enters the engine layer
 * functions via the engine switch. Please see @ref eng_switch_sha_ops
 *
 * When possible this will forward the data directly to the engine layer
 * for HW processing. When that is not possible it will gather the data into a
 * context buffer (sha_context->buf) so that it complies with the HW requirements.
 *
 * This slows down the SHA processing, so it is recommended that all input
 * should be that caller makes sure these two things hold:
 * -# passed in contiguous buffers
 * -# TE_OP_UPDATE passes in only data chunk sizes which are multiples
 *    of the used SHA algorithm internal block size. The SE HW engine
 *    only produces intermediate results when condition #2 is true.
 *
 * se_sha_process_input() uses the following fields from input_params:
 * - src        : address of chunk to digest
 * - input_size : number of bytes in this chunk to digest
 *
 * The following values used only with TE_OP_DOFINAL (is_last == true):
 * - dst        : digest output buffer
 * - output_size: size of dst buffer [IN], number of digest bytes written [OUT]
 *
 * @param input_params input/output object with buffer addresses and sizes.
 * @param sha_context SHA context
 *
 * @return NO_ERROR on success, error code otherwise.
 *
 * @ingroup process_layer_api
 * @defgroup process_layer_md_api Process layer message digest handler
 */
/*@{*/
status_t se_sha_process_input(
	struct se_data_params *input_params,
	struct se_sha_context *sha_context);

#if HAVE_SE_ASYNC_SHA
/**
 * @brief Checks if engine is free after started async SHA operation
 *
 * The async process versions are used if the TE_OP_ASYNC_UPDATE_START
 * or TE_OP_ASYNC_DOFINAL_START are used as the starting primitives in the
 * context api SHA calls. If so, the TE_OP_ASYNC_CHECK_STATE can be used to
 * check engine state and TE_OP_ASYNC_FINISH to finalize both started update
 * and started dofinal async operations.
 *
 * @param sha_context SHA context
 * @param done_p referenced object will be set true if engine SHA engine is done.
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t se_sha_process_async_check(
	const struct se_sha_context *sha_context,
	bool *done_p);

/**
 * @brief Async process layer handler for SHA engine related operations.
 * Handle async SHA digest data processing ops (update,dofinal)
 *
 * Do an asynchronous SHA start if sha_context->async_start is true,
 * otherwise does a blocking finalize call which blocks until the SHA engine
 * is done.
 *
 * Caller is responsible of triggering the finalize operations after starting
 * the SHA asynchronously.
 *
 * The non-blocking function se_sha_process_async_check() can be used
 * to check current state of engine after the async sha operation has
 * been started.
 *
 * This is an asynchronous version of the synchronous se_sha_process_input()
 * function.
 *
 * input_params can be NULL if finishing an async operation (it is not used
 * in that case), all input has been provided in the async start.
 *
 * If this is used to start an async update (is_last == false) then the
 * finalize completes that, not the SHA context operation. So a
 * dofinal operation is required to complete the SHA context operation
 * after the async
 *
 * The async process versions are used if the TE_OP_ASYNC_UPDATE_START
 * or TE_OP_ASYNC_DOFINAL_START are used as the starting primitives in the
 * context api SHA calls. If so, the TE_OP_ASYNC_CHECK_STATE can be used to
 * check engine state and TE_OP_ASYNC_FINISH to finalize both started update
 * and started dofinal async operations.
 *
 * @param input_params input/output object with buffer addresses and sizes.
 * @param sha_context SHA context
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t se_sha_process_async_input(
	struct se_data_params *input_params,
	struct se_sha_context *sha_context);
#endif /* HAVE_SE_ASYNC_SHA */
/*@}*/
#endif /* HAVE_SE_SHA */

#if HAVE_SE_AES
/**
 * @brief process layer handler for all AES engine related operations.
 *
 * This function is called by (e.g.) the CLASS handler for AES algorithms
 * to process data passed in the TE_OP_UPDATE and TE_OP_DOFINAL operations.
 *
 * When possible this will forward the data directly to the engine layer
 * for HW processing. When that is not possible it will gather the data into a
 * context buffer (sha_context->buf) so that it complies with the HW requirements.
 *
 * This slows down the AES processing, so it is recommended that all input
 * should be that caller makes sure these two things hold:
 * -# passed in contiguous buffers
 * -# TE_OP_UPDATE passes in only data chunk sizes which are multiples
 *    of the AES algorithm internal block size (N * 16 bytes).
 *
 * se_aes_process_input() uses the following fields from input_params:
 * - src        : address of chunk to process
 * - input_size : number of chunk bytes to process
 * - dst        : result output buffer
 * - output_size: size of dst buffer [IN], number of result bytes written [OUT]
 *
 * @param input_params input/output object with buffer addresses and sizes.
 * @param sha_context AES context
 *
 * @return NO_ERROR on success, error code otherwise.
 *
 * @ingroup process_layer_api
 * @defgroup process_layer_cipher_api Process layer symmetric cipher handler
 */
/*@{*/
status_t se_aes_process_input(
	struct se_data_params *input_params,
	struct se_aes_context *aes_context);

#if HAVE_SE_ASYNC_AES
/**
 * @brief Checks if engine is free after started async AES operation
 *
 * The async process versions are used if the TE_OP_ASYNC_UPDATE_START
 * or TE_OP_ASYNC_DOFINAL_START are used as the starting primitives in the
 * context api AES calls. If so, the TE_OP_ASYNC_CHECK_STATE can be used to
 * check engine state and TE_OP_ASYNC_FINISH to finalize both started update
 * and started dofinal async operations.
 *
 * @param aes_context AES context
 * @param done_p referenced object will be set true if engine AES engine is done.
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t se_aes_process_async_check(
	const struct se_aes_context *aes_context,
	bool *done_p);

/**
 * @brief Async process layer handler for AES engine related operations.
 * Handle async AES cipher data processing ops (update,dofinal)
 *
 * Do an asynchronous AES start if aes_context->async_start is true,
 * otherwise does a blocking finalize call which blocks until the AES engine
 * is done.
 *
 * Caller is responsible of triggering the finalize operations after starting
 * the AES asynchronously.
 *
 * The non-blocking function se_aes_process_async_check() can be used
 * to check current state of engine after the async aes operation has
 * been started.
 *
 * This is an asynchronous version of the synchronous se_aes_process_input()
 * function.
 *
 * input_params can be NULL if finishing an async operation (it is not used
 * in that case), all input has been provided in the async start.
 *
 * If this is used to start an async update (is_last == false) then the
 * finalize completes that, not the AES context operation. So a
 * dofinal operation is required to complete the AES context operation
 * after the async
 *
 * The async process versions are used if the TE_OP_ASYNC_UPDATE_START
 * or TE_OP_ASYNC_DOFINAL_START are used as the starting primitives in the
 * context api AES calls. If so, the TE_OP_ASYNC_CHECK_STATE can be used to
 * check engine state and TE_OP_ASYNC_FINISH to finalize both started update
 * and started dofinal async operations.
 *
 * @param input_params input/output object with buffer addresses and sizes.
 * @param aes_context AES context
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t se_aes_process_async_input(
	struct se_data_params *input_params,
	struct se_aes_context *aes_context);
#endif /* HAVE_SE_ASYNC_AES */
/*@}*/

#if HAVE_CMAC_AES
/**
 * @brief Plain AES block cipher (i.e. AES-ECB) support function
 * for other algorithms.
 *
 * AES-ECB should not be used for ciphering multi-block buffers,
 * verify each use case by case for security concerns!
 *
 * This function is used used when supporting standardized
 * operations for standard algorithms, like CMAC subkey derivation
 * or AES GCM final block handling (in hybrid versions).
 *
 * This function only operates on single AES block input/output sizes!
 * Exported only for CCC internal use cases.
 *
 * NOTE: This requests/releases memory for AES engine context at runtime.
 *
 * @param econtext AES engine context
 * @param aes_out AES-ECB output buffer (size 16 bytes)
 * @param aes_in AES-ECB input buffer (size 16 bytes)
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t aes_ecb_encrypt_block(
	struct se_engine_aes_context *econtext,
	uint8_t *aes_out,
	const uint8_t *aes_in);

/**
 * @brief process layer handler for CMAC-AES operation using the AES engine.
 *
 * This function is called by (e.g.) the CLASS handler for CMAC-AES algorithm
 * to process data passed in the TE_OP_UPDATE and TE_OP_DOFINAL operations.
 *
 * When possible this will forward the data directly to the engine layer
 * for HW processing. When that is not possible it will gather the data into a
 * context buffer (aes_context->buf) so that it complies with the HW requirements.
 *
 * This slows down the CMAC-AES processing, so it is recommended that all input
 * should be that caller makes sure these two things hold:
 * -# passed in contiguous buffers
 * -# TE_OP_UPDATE passes in only data chunk sizes which are multiples
 *    of the AES algorithm internal block size (N * 16 bytes).
 *
 * se_cmac_process_input() uses the following fields from input_params:
 * - src        : address of chunk to process
 * - input_size : number of chunk bytes to process
 * - dst        : result output buffer
 * - output_size: size of dst buffer [IN], number of result bytes written [OUT]
 *
 * @param input_params input/output object with buffer addresses and sizes.
 * @param aes_context AES context
 *
 * @return NO_ERROR on success, error code otherwise.
 *
 * @ingroup process_layer_api
 * @defgroup process_layer_mac_api Process layer MAC handler
 */
/*@{*/
status_t se_cmac_process_input(
	struct se_data_params *input_params,
	struct se_aes_context *aes_context);

/**
 * @brief Callwlate the CMAC subkeys K1, K2
 *
 * Derives the CMAC "subkeys" which are used for last block
 * handling in CMAC-AES.
 *
 * @param aes_context AES context
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t cmac_process_derive_subkeys(struct se_aes_context *aes_context);
/*@}*/
#endif /* HAVE_CMAC_AES */
#endif /* HAVE_SE_AES */

#if CCC_WITH_DRNG || HAVE_PKA1_TRNG
/**
 * @brief process layer handler for RNG operation using the HW engines
 * to generate random numbers.
 *
 * This function is called by (e.g.) the CLASS handler
 * to generate random numbers to the buffer passed in the TE_OP_DOFINAL operation.
 *
 * There is no support for TE_OP_UPDATE in random number generator class so
 * this is only called by TE_OP_DOFINAL completing the operation.
 *
 * This can generate random numbers with three different HW engines:
 * - RNG1 device (DRNG)
 * - SE device AES0 engine random number generator (DRNG)
 * - PKA1 device TRNG engine (TRNG)
 *
 * TODO: Disable TRNG access to PKA1 true random number generator.
 * It should not be used by apps. Only should allow for R&D, e.g. for
 * testing TRNG entropy quality.
 *
 * se_random_process() uses the following fields from input_params:
 * - dst        : digest output buffer
 * - output_size: size of dst buffer (buffer filled with output_size RNG bytes)
 *
 * @param input_params input/output object with buffer addresses and sizes.
 * @param rng_context RNG context
 *
 * @return NO_ERROR on success, error code otherwise.
 *
 * @ingroup process_layer_api
 * @defgroup process_layer_rng_api Process layer RNG handler
 */
/*@{*/
status_t se_random_process(
	struct se_data_params *input_params,
	struct se_rng_context *rng_context);

#if defined(NEED_SE_CRYPTO_GENERATE_RANDOM)
/**
 * @brief Helper function for generating DRGN random numbers
 * for the kernel context requests with the SE device AES0 engine
 *
 * NOTE: SE mutex can not be held when calling this!
 *
 * Should use rng1_generate_drng() instead in all cases when RNG1
 * device is enabled.
 *
 * @param buf Buffer to fill with bytes from SE device AES0 DRNG
 * @param buflen Byte length of buf
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t se_crypto_kernel_generate_random(uint8_t *buf, uint32_t buflen);
#endif /* defined(NEED_SE_CRYPTO_GENERATE_RANDOM) */
/*@}*/
#endif /* CCC_WITH_DRNG || HAVE_PKA1_TRNG */

#if HAVE_RNG1_DRNG
/**
 * @brief Call this to fetch randomness to buffer when CCC is used as library
 * for "kernel code" (same address space calls).
 *
 * This manages the mutex and selects the proper engine internally and
 * can be called with or without SE mutex or PKA1 mutex held.
 *
 * NOTE: This function uses the RNG1 mutex locks directly (does not use the
 * HW_MUTEX_TAKE_ENGINE/HW_MUTEX_RELEASE_ENGINE macros) so that this function
 * can be always called no matter what other mutexes are held by the caller.
 *
 * This requires that the RNG1 mutex is not granted to anyone if it is lwrrently
 * held (by anyone)! I think the HW works this way => please let me know if
 * you disagree!
 *
 * If this is not true, then operations that already hold some mutex would be
 * required to call DRNG locked function protected by the same mutex: SE => SE0_DRNG,
 * PKA1 => TRNG and RNG1 could be only called when no mutexes are held (as the
 * macros lock the "CCC system SW mutex", they prevent such cross-mutex calls!)
 *
 * @param dbuf Buffer to fill with RNG1 DRNG bytes
 * @param dbuf_len Byte length of dbuf
 *
 * @return NO_ERROR on success, error code otherwise.
 *
 * @ingroup process_layer_rng_api
 */
/*@{*/
status_t rng1_generate_drng(uint8_t *dbuf, uint32_t dbuf_len);
/*@}*/
#endif /* HAVE_RNG1_DRNG */

#if HAVE_PKA1_TRNG
#if defined(NEED_PKA1_TRNG_GENERATE_RANDOM)
/**
 * @brief fetch TRNG bytes to buffer when CCC is used as library
 * for "kernel code" (same address space calls).
 *
 * NOTE: The PKA1 TRNG engine should not be used by applications.
 *
 * This function only exists for R&D purposes e.g. for generating
 * entropy for quality tests.
 *
 * @param buf Buffer to fill with bytes from PKA1 device TRNG engine
 * @param buflen Byte length of buf
 *
 * @return NO_ERROR on success, error code otherwise.
 *
 * @ingroup process_layer_rng_api
 */
/*@{*/
status_t pka1_trng_generate_random(uint8_t *buf, uint32_t size);
/*@}*/
#endif /* defined(NEED_PKA1_TRNG_GENERATE_RANDOM) */
#endif /* HAVE_PKA1_TRNG */

#if HAVE_LWRNG_DRNG
/**
 * @brief Call this to fetch randomness to buffer when CCC is used as library
 * for "kernel code" (same address space calls).
 *
 * @param dbuf Buffer to fill with LWRNG DRNG bytes
 * @param dbuf_len Byte length of dbuf
 *
 * @return NO_ERROR on success, error code otherwise.
 *
 * @ingroup process_layer_rng_api
 */
/*@{*/
status_t lwrng_generate_drng(uint8_t *dbuf, uint32_t dbuf_len);
/*@}*/
#endif /* HAVE_LWRNG_DRNG */

/***************************************/

#if HAVE_SW_WHIRLPOOL
/**
 * @brief process layer handler for WHIRLPOOL v3 SW implementation (HAVE_SW_WHIRLPOOL=1)
 *
 * NOTE: WHIRLPOOL algorithm code is not in CCC TOT, so CCC
 * does not be compiled to support WHIRLPOOL; it is in R&D versions only).
 *
 * This function is called by (e.g.) the CLASS handler for message digests
 * to process data passed in the TE_OP_UPDATE and TE_OP_DOFINAL operations.
 *
 * When possible this will forward the data directly to the engine layer
 * for HW processing. This is always possible for "SW engines" so
 * SW digests do not buffer any data.
 *
 * soft_whirlpool_process_input() uses the following fields from input_params:
 * - src        : address of chunk to digest
 * - input_size : number of bytes in this chunk to digest
 *
 * The following values used only with TE_OP_DOFINAL (is_last == true):
 * - dst        : digest output buffer
 * - output_size: size of dst buffer [IN], number of digest bytes written [OUT]
 *
 * @param input_params input/output object with buffer addresses and sizes.
 * @param wpool_context WHIRLPOOL soft context
 *
 * @return NO_ERROR on success, error code otherwise.
 *
 * @ingroup process_layer_md_api
 */
/*@{*/
struct soft_whirlpool_context;

status_t soft_whirlpool_process_input(
	struct se_data_params *input_params,
	struct soft_whirlpool_context *wpool_context);
/*@}*/
#endif /* HAVE_SW_WHIRLPOOL*/

#if HAVE_MD5
/**
 * @brief process layer handler for MD5 SW implementation (HAVE_MD5=1)
 *
 * NOTE: MD5 is a weak digest algorithm, use this only for R&D purposes,
 * not in production code. (MD5 algorithm code is not in CCC TOT, so CCC
 * does not be compiled to support MD5; it is in R&D versions only).
 *
 * This function is called by (e.g.) the CLASS handler for message digests
 * to process data passed in the TE_OP_UPDATE and TE_OP_DOFINAL operations.
 *
 * When possible this will forward the data directly to the engine layer
 * for HW processing. This is always possible for "SW engines" so
 * SW digests do not buffer any data.
 *
 * soft_md5_process_input() uses the following fields from input_params:
 * - src        : address of chunk to digest
 * - input_size : number of bytes in this chunk to digest
 *
 * The following values used only with TE_OP_DOFINAL (is_last == true):
 * - dst        : digest output buffer
 * - output_size: size of dst buffer [IN], number of digest bytes written [OUT]
 *
 * @param input_params input/output object with buffer addresses and sizes.
 * @param md5_context md5 soft context
 *
 * @return NO_ERROR on success, error code otherwise.
 *
 * @ingroup process_layer_md_api
 */
/*@{*/
struct soft_md5_context;

status_t soft_md5_process_input(
	struct se_data_params *input_params,
	struct soft_md5_context *md5_context);
/*@}*/
#endif /* HAVE_MD5 */

#if HAVE_SW_SHA3
/**
 * @brief process layer handler for SHA-3 SW implementation (HAVE_SW_SHA3=1)
 *
 * NOTE: SHA-3 algorithm code is not in CCC TOT, so CCC
 * does not be compiled to support WHIRLPOOL; it is in R&D versions only).
 *
 * This function is called by (e.g.) the CLASS handler for message digests
 * to process data passed in the TE_OP_UPDATE and TE_OP_DOFINAL operations.
 *
 * When possible this will forward the data directly to the engine layer
 * for HW processing. This is always possible for "SW engines" so
 * SW digests do not buffer any data.
 *
 * soft_sha3_process_input() uses the following fields from input_params:
 * - src        : address of chunk to digest
 * - input_size : number of bytes in this chunk to digest
 *
 * The following values used only with TE_OP_DOFINAL (is_last == true):
 * - dst        : digest output buffer
 * - output_size: size of dst buffer [IN], number of digest bytes written [OUT]
 *
 * @param input_params input/output object with buffer addresses and sizes.
 * @param sha3_context SHA-3 soft context
 *
 * @return NO_ERROR on success, error code otherwise.
 *
 * @ingroup process_layer_md_api
 */
/*@{*/
struct soft_sha3_context; /**< soft context for SW SHA-3 */

status_t soft_sha3_process_input(
	struct se_data_params *input_params,
	struct soft_sha3_context *sha3_context);
/*@}*/
#endif /* HAVE_SW_SHA3 */

/***************************************/

#if CCC_WITH_ECC
#if CCC_WITH_ECDH || CCC_WITH_X25519
/**
 * @brief process layer handler for ECDH derivation operations.
 *
 * This function is called by (e.g.) the CLASS handler for ECDH derivation
 * algorithm to process data passed in the TE_OP_DOFINAL operations.
 *
 * This is used for ECDH and X25519 (ECDH with lwrve25519) when enabled.
 *
 * There is no support for TE_OP_UPDATE in any "EC engine" related
 * class operations so this is only called by TE_OP_DOFINAL completing
 * the operation in this call.
 *
 * ECDH shared secret callwlation
 * ------------------------------
 *
 * To callwlate ECDH we need a scalar value S (in ec_pkey) and
 * some some lwrve point P => ECDH(S,P) == S x P
 *
 * If SRC == NULL => callwlate ECDH value with the scalar Da (in
 *  ec_pkey) and my public key point Pmy (where Pmy = Da x G (G is
 *  lwrve generator point)) This case is not very useful, since the
 *  result can only be callwlated by knowing the private key. [ But
 *  this could also be used for deriving keys for symmetric ciphers,
 *  etc... ]
 *
 * If SRC != NULL => SRC is the peer public key point (in SRC_POINT)
 *  and scalar is my private key (Da in ec_pkey): ECDH(Da,SRC_POINT) =
 *  Da x SRC_POINT In this case SRC_POINT = peer_secret_key(PPk) x G
 *
 * In this case the ECDH generates a persistent shared secret between
 *  parties, if peer also (safely) knows my public key (Pmy):
 *
 *  ECDH(Da x SRC_POINT) = ECDH(PPk x Pmy) == persistent_shared_secret (pss)
 *
 * Set the CCC_EC_FLAGS_BIG_ENDIAN_KEY flag is ec_pkey is big endian,
 * LE by default.
 *
 * se_ecdh_process_input() uses the following fields from input_params:
 * - point_P    : public key (point P to use for ECDH). If NULL, derive public key
 *                with ec_pkey*generator point and use that as public key.
 * - dst        : shared secret
 * - output_size: size of dst buffer [IN], nnumber of bytes written [OUT]
 *
 * @param input_params input/output values for ECDH
 * @param ec_context EC context (with ec_pkey set)
 *
 * @return NO_ERROR on success, error code otherwise.
 *
 * @ingroup process_layer_api
 * @defgroup process_layer_derive_api Process layer key derivation handler
 */
/*@{*/
status_t se_ecdh_process_input(
	struct se_data_params *input_params,
	struct se_ec_context *ec_context);
/*@}*/
#endif /* CCC_WITH_ECDH || CCC_WITH_X25519 */

/** Switches to correct fun according to lwrve type
 *
 * @ingroup process_layer_asig_api
 */
/*@{*/
status_t se_ecc_process_verify(
	struct se_data_params *input_params,
	struct se_ec_context *ec_context);

/* Switches to correct fun according to lwrve type */
status_t se_ecc_process_sign(
	struct se_data_params *input_params,
	struct se_ec_context *ec_context);
/*@}*/

/**
 * @brief ECDSA signature verification
 *
 * For ECDSA signatures the signature contains two bignum values:
 * ESIG=(r,s) both of which have the size of the EC lwrve in bytes.
 * To verify ESIG the public key Qa is required.
 *
 * The signature input parameter format is represented as a const te_ec_sig_t
 *
 * ECDSA sig verification:
 * -----------------------
 *
 * As in wikipedia: must first validate the pubkey and then verify the signature =>
 *
 * A) Pubkey validation
 *    1) check that Qa is not equal to the identity element (O_ie) and coordinates are valid
 *    2) check that Qa is on lwrve
 *    3) check that n x Qa == Oie
 *
 * B) Sig verification
 *    1) Verify that r and s are integers in [1,n-1] (inclusive).
 *    2) e = HASH(m) (=> in input_params->sig_digest)
 *    3) z = Ln bits of e (Ln is the bitlength of the group order n)
 *    4) w = s^-1 (mod n)
 *    5) u1 = z * w (mod n) and u2 = r * w (mod n)
 *    6) R(x1,y1) = u1 x G + u2 x Qa (use Shamir's trick (if possible, Verify if restrictions apply!!!))
 *    7) signature is valid if => r == x1 (mod n)
 *
 * Uses input_params fields:
 * - src_digest: digest to verify
 * - src_digest_size: digest size in bytes
 * - src_ec_signature: ECDSA signature blob (ASN.1 formatted as in OpenSSL
 *   or <R,S,flags> CCC type struct te_ec_sig_s)
 * - src_signature_size: byte size of signature
 *
 * @param input_params input/output values for ECDSA
 * @param ec_context EC context
 *
 * @return NO_ERROR on success, error code otherwise.
 *
 * @ingroup process_layer_asig_api
 */
/*@{*/
status_t se_ecdsa_verify(
	struct se_data_params *input_params,
	struct se_ec_context *ec_context);
/*@}*/

/**
 * @brief ECDSA signing (FUNCTION NOT IMPLEMENTED YET)
 *
 * Note: For signing it is ESSENTIAL that the value k is unique per signature.
 * If not: ECDSA can be broken, key k can be attacked.
 *
 * Can be achieved by both proper random number generator and by deterministic value generator,
 * (e.g. SHA2(concat(EC private key + message digest))).
 *
 * XXX I think I would use the latter, or then make it configurable (or just XOR together the
 * XXX above SHA2 result and a DRNG generated by the SE random number generator).
 *
 * To generate a signature we need:
 *  Da			(private key)
 *  e			(message digest)
 *  lwrve		(lwrve parameters)
 *
 * ECDSA sig generation:
 * ---------------------
 *
 * 1) e = HASH(m) (=> in input_params->sig_digest)
 * 2) z = Ln bits of e (Ln is the bitlength of the group order n. Ln may be larger than n, but not longer)
 * 3) select value k from [1,n-1] (inclusive; this MUST BE UNIQUE and CRYPTOGRAPHICALLY SECURE)
 * 4) (x1,y1) = k x G
 * 5) r = x1 (mod n)			(if r == 0 => goto 3)
 * 6) s = k^-1 * (z + r * Da) (mod n)	(if s == 0 => goto 3)
 * 7) signature is (r,s)
 *
 * @param input_params input/output values for ECDSA signing
 * @param ec_context EC context
 *
 * @returns ERR_NOT_IMPLEMENTED this operation is nt yet implemented.
 *
 * @ingroup process_layer_asig_api
 */
/*@{*/
status_t se_ecdsa_sign(
	struct se_data_params *input_params,
	struct se_ec_context *ec_context);
/*@}*/

#if CCC_WITH_ED25519
#if HAVE_ED25519_SIGN
/**
 * @brief Used by crypto_asig_proc.c when encoding ED25519 private key scalar value
 *
 * Overwrites the bytes [0..31] of the in-kernel copy
 * ec_key->key field with the encoded value.
 *
 * Value "h" used for signature creation (last 32 bytes of
 * SHA512 digest are stored to the bytes key[32..63].
 *
 * @param econtext ec engine context
 * @param a NULL or [out] ed25519 encoded scalar value
 * @param h NULL or [out] last 32 bytes of SHA-512 message digest
 * @param pkey pkey for ed25519 (digested) [in]
 * @param nbytes pkey byte length
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t ed25519_encode_scalar(const se_engine_ec_context_t *econtext,
			       uint8_t *a, uint8_t *h,
			       const uint8_t *pkey, uint32_t nbytes);
#endif /* HAVE_ED25519_SIGN */

/**
 * @brief ED25519 signature verification.
 *
 * @param input_params input/output values for ED25519
 * @param ec_context EC context
 *
 * @return NO_ERROR on success, error code otherwise.
 *
 * @ingroup process_layer_asig_api
 */
/*@{*/
status_t se_ed25519_verify(
	struct se_data_params *input_params,
	struct se_ec_context *ec_context);

/**
 * @brief ED25519 signing
 *
 * @param input_params input/output values for ED25519 signing
 * @param ec_context EC context
 *
 * @returns ERR_NOT_IMPLEMENTED this operation is nt yet implemented.
 */
status_t se_ed25519_sign(
	struct se_data_params *input_params,
	struct se_ec_context *ec_context);
/*@}*/
#endif /* CCC_WITH_ED25519 */

struct pka1_ec_lwrve_s;

/** @brief CCC internal support functions
 */
#if CCC_SOC_WITH_PKA1
#define USE_SMALLER_VEC_GEN (HAVE_PKA1_BLINDING || HAVE_GENERATE_EC_PRIVATE_KEY || HAVE_ECDSA_SIGN)
#else
#define USE_SMALLER_VEC_GEN (HAVE_GENERATE_EC_PRIVATE_KEY || HAVE_ECDSA_SIGN)
#endif

#if USE_SMALLER_VEC_GEN
status_t pka_ec_generate_smaller_vec_BE_locked(const engine_t *engine,
					       const uint8_t *boundary,
					       uint8_t *gen_value, uint32_t len,
					       bool boundary_LE);

#if HAVE_GENERATE_EC_PRIVATE_KEY
/** @brief Generate EC private key for a lwrve (if enabled)
 * Key must be in range [ 1 .. n-1 ] (inclusive) where n is the lwrve order.
 *  Generated key is in big endian.
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t pka_ec_generate_pkey_BE(const engine_t *engine,
				 const struct pka1_ec_lwrve_s *lwrve,
				 uint8_t *key, uint32_t klen);
#endif
#endif /* USE_SMALLER_VEC_GEN */

/** @brief CCC support: locked check if point is on lwrve (PKA1 mutex held)
 */
status_t ec_check_point_on_lwrve_locked(se_engine_ec_context_t *econtext,
					const struct pka1_ec_lwrve_s *lwrve,
					const te_ec_point_t *point);

/** @brief CCC support: check if point is on lwrve (takes PKA1 mutex).
 */
status_t ec_check_point_on_lwrve(se_engine_ec_context_t *econtext,
				 const struct pka1_ec_lwrve_s *lwrve,
				 const te_ec_point_t *point);

/** @brief CCC support: multiply point with a scalar.
 *
 * @param econtext EC engine context
 * @param R result point
 * @param P source point
 * @param s scalar value
 * @param slen byte length of scalar s
 * @param s_LE True if s is in little endian order
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t ec_point_multiply(se_engine_ec_context_t *econtext,
			   te_ec_point_t *R, const te_ec_point_t *P,
			   const uint8_t *s, uint32_t slen,
			   bool s_LE);

/** @brief CCC support: multiply point with a scalar, PKA1 mutex held
 *
 * @param econtext EC engine context
 * @param R result point
 * @param P source point
 * @param s scalar value
 * @param slen byte length of scalar s
 * @param s_LE True if s is in little endian order
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t ec_point_multiply_locked(se_engine_ec_context_t *econtext,
				  te_ec_point_t *R, const te_ec_point_t *P,
				  const uint8_t *s, uint32_t slen,
				  bool s_LE);

/** @brief CCC support: callwlate R=u1_k*P1+u2_l*P2
 *
 * If P1 is NULL, use lwrve generator point as P1.
 *
 * If HAVE_SHAMIR_TRICK=1 use HW shamir's trick support, otherwise
 * do it the longer way.
 *
 * The scalars u1_k and u2_l endianess specified in ec_scalar_flags,
 * big endian by default.
 *
 * @param econtext EC engine context
 * @param R result point
 * @param u1_k scalar k
 * @param P1 source point
 * @param u2_l scalar l
 * @param P2 source point
 * @param ec_scalar_flags flags to specify endianess of u1_k and u2_l
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t callwlate_shamirs_value(se_engine_ec_context_t *econtext,
				 te_ec_point_t *R,
				 const uint8_t *u1_k,
				 const te_ec_point_t *P1,
				 const uint8_t *u2_l,
				 const te_ec_point_t *P2,
				 uint32_t ec_scalar_flags);
#endif /* CCC_WITH_ECC*/

/***************************************/

#if CCC_WITH_RSA
#if HAVE_RSA_CIPHER
/**
 * @brief process layer handler for RSA asymmetric cipher operations.
 *
 * This function is called by (e.g.) the CLASS handler for RSA asymmetric cipher
 * algorithms to process data passed in the TE_OP_DOFINAL operations.
 *
 * This is used for e.g. RSA-OAEP and RSA-PKCS#1v1.5 encrypt/decrypt operations,
 * not for signature related processing.
 *
 * There is no support for TE_OP_UPDATE in any "RSA engine" related
 * class operations so this is only called by TE_OP_DOFINAL completing
 * the operation in this call.
 *
 * se_rsa_process_input() uses the following fields from input_params:
 * - src        : address of data to provide for RSA cipher/decipher
 * - input_size : number of bytes to process (RSA modulus size or error)
 * - dst        :
 * - output_size: size of dst buffer [IN], number of digest bytes written [OUT]
 *
 * @param input_params input/output object with buffer addresses and sizes.
 * @param rsa_context RSA context
 *
 * @return NO_ERROR on success, error code otherwise.
 *
 * @ingroup process_layer_api
 * @defgroup process_layer_acipher_api Process layer asymmetric cipher handler
 */
/*@{*/
status_t se_rsa_process_input(
	struct se_data_params *input_params,
	struct se_rsa_context *rsa_context);
/*@}*/
#endif /* HAVE_RSA_CIPHER */

/**
 * @brief process layer handler for RSA signature verifications.
 *
 * This function is called by (e.g.) the CLASS handler for RSA signature verifications
 * algorithms to process data passed in the TE_OP_DOFINAL operations.
 *
 * This is used for e.g. RSA-PSS and RSA-PKCS#1v1.5 sig verifications,
 * not for RSA cipher or signing operations.
 *
 * There is no support for TE_OP_UPDATE in any "RSA engine" related
 * class operations so this is only called by TE_OP_DOFINAL completing
 * the operation in this call.
 *
 * se_rsa_process_verify() uses the following values from input_params:
 *
 * When input is a digest to verify:
 * - src_digest : address of digest to verify
 * - src_digest_size : size of digest
 * - src_signature : signature to verify
 * - src_signature_size: byte size of signature
 *
 * When input is a message to verify:
 * - src : address of message to verify
 * - input_size : size of message
 * - src_signature : signature to verify
 * - src_signature_size: byte size of signature
 *
 * The call will verify a DIGEST unless the flag
 * INIT_FLAG_RSA_DIGEST_INPUT is set in the TE_OP_INIT primitive
 * rsa initialization flags. In set: the input message is first
 * digested by the selected algorithm implied SHA digest
 * and the callwlated message digest is verified with the provided signature.
 *
 * @param input_params input/output object with buffer addresses and sizes.
 * @param rsa_context RSA context
 *
 * @return NO_ERROR on success, error code otherwise.
 *
 * @ingroup process_layer_api
 * @defgroup process_layer_asig_api Process layer signature verification handler
 */
/*@{*/
status_t se_rsa_process_verify(
	struct se_data_params *input_params,
	struct se_rsa_context *rsa_context);
/*@}*/

/**
 * @brief process layer handler for RSA signing.
 *
 * This function is called by (e.g.) the CLASS handler for RSA signing
 * algorithms to process data passed in the TE_OP_DOFINAL operations.
 *
 * This is used for e.g. RSA-PSS and RSA-PKCS#1v1.5 signing,
 * not for RSA cipher or signature verification operations.
 *
 * There is no support for TE_OP_UPDATE in any "RSA engine" related
 * class operations so this is only called by TE_OP_DOFINAL completing
 * the operation in this call.
 *
 * se_rsa_process_sign() uses the following values from input_params:
 *
 * When input is a digest to sign:
 * - src_digest : address of digest
 * - src_digest_size : size of digest
 * - dst : buffer for created signature
 * - output_size: byte size of output buffer
 *
 * When input is a message to sign:
 * - src : address of message
 * - input_size : size of message
 * - dst : buffer for created signature
 * - output_size: byte size of output buffer
 *
 * The call will sign a DIGEST unless the flag
 * INIT_FLAG_RSA_DIGEST_INPUT is set in the TE_OP_INIT primitive
 * rsa initialization flags. In set: the input message is first
 * digested by the selected algorithm implied SHA digest
 * and the callwlated message digest is signed with the provided signature.
 *
 * @param input_params input/output object with buffer addresses and sizes.
 * @param rsa_context RSA context
 *
 * @return NO_ERROR on success, error code otherwise.
 *
 * @ingroup process_layer_asig_api
 */
/*@{*/
status_t se_rsa_process_sign(
	struct se_data_params *input_params,
	struct se_rsa_context *rsa_context);
/*@}*/

/**
 * @brief RSA signature mask generation function
 *
 * Internal function for RSA signature handling.
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t se_mgf_mask_generation(
	struct context_mem_s *cmem,
	const uint8_t *in_seed, uint32_t in_seed_len,
	uint8_t *out_mask_buffer, uint32_t out_mask_len,
	te_crypto_algo_t hash_algorithm, uint32_t hlen,
	uint8_t *cbuffer, uint32_t cbuf_len,
	const uint8_t *mask_to_xor);

/** @brief Verify rsa data < modulus.
 * Data length must be RSA_SIZE here: caller verified.
 *
 * This can only be checked in case the modulus exists,
 * this is false only when a pre-loaded key is used.
 *
 * Even so the modulus may still exist: if client has provided it and
 * set a proper flag => in that case RSAVP1 s < (n) comparison
 * is performed even in that case.
 *
 * @param econtext rsa engine context
 * @param data Data to verify
 * @param big_endian data is big endian
 *
 * @return NO_ERROR on success, error code otherwise.
 * ERR_TOO_BIG if data value is not less than modulus.
 * ERR_BAD_STATE on state errors.
 */
status_t rsa_verify_data_range(const struct se_engine_rsa_context *econtext,
			       const uint8_t *data,
			       bool big_endian);

#if HAVE_RSA_PKCS_VERIFY || HAVE_RSA_PKCS_SIGN
/**
 * @brief PKCS#1v1.5 EM encoder as in RFC-8017
 *
 * @param input_params src_digest/src_digest_size fields used for em
 * @param rsa_context RSA context
 * @param em encoded message
 * @param is_verify Operation is signature verify (not cipher)
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t rsa_sig_pkcs1_v1_5_encode(
	const struct se_data_params *input_params,
	const struct se_rsa_context *rsa_context,
	uint8_t *em,
	bool is_verify);
#endif /* HAVE_RSA_PKCS_VERIFY || HAVE_RSA_PKCS_SIGN */

#if HAVE_RSA_SIGN
/**
 * @brief RSA sign digest or message
 *
 * When input is a digest to sign:
 * - src_digest : address of digest to sign
 * - src_digest_size : size of digest
 * - dst : generated signature
 * - output_size: dst buffer size
 *
 * When input is a message to verify:
 * - src : address of message to sign
 * - input_size : size of message
 * - dst : generated signature
 * - output_size: dst buffer size
 *
 * The call will sign a DIGEST unless the flag
 * INIT_FLAG_RSA_DIGEST_INPUT is set in the TE_OP_INIT primitive
 * rsa initialization flags. In set: the input message is first
 * digested by the selected algorithm implied SHA digest
 * and the callwlated message digest is signed with the provided signature.
 *
 * @param input_params data params for RSA signing
 * @param rsa_context RSA context
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t rsa_process_sign(struct se_data_params *input_params,
			  struct se_rsa_context *rsa_context);
#endif /* HAVE_RSA_SIGN */

#if HAVE_CRYPTO_KCONFIG || defined(SE_RSA_PREDEFINED_DYNAMIC_KEYSLOT)
/** @brief find out a dynamic RSA keyslot for an RSA operation.
 *
 * For PKA1 engine the dynamic keyslot is not defined; the caller is
 * expected to always use the PKA1 bank registers instead when a
 * dynamic keyslot is needed (returns an invalid keyslot value with
 * NO_ERROR in that case).
 *
 * For PKA0 a dynamic keyslot may be needed and the context keyslot is
 * returned if defined. If not defined, try to find a configured
 * keyslot instead.
 *
 * RSA engine context field rsa_dynamic_keyslot is set only by the
 * subsystem using CCC.
 *
 * If the flag RSA_FLAGS_DYNAMIC_KEYSLOT is set, that keyslot can be
 * freely used by CCC for all RSA operations related to this context.
 *
 * @param econtext rsa engine context
 * @param dynamic_keyslot_p reference set to dynamic keyslot number for SE PKA0
 * @return NO_ERROR on success, error code otherwise.
 */
status_t pka0_engine_get_dynamic_keyslot(const struct se_engine_rsa_context *econtext,
					 uint32_t *dynamic_keyslot_p);
#endif /* HAVE_CRYPTO_KCONFIG || defined(SE_RSA_PREDEFINED_DYNAMIC_KEYSLOT) */
#endif /* CCC_WITH_RSA */

/***************************************/

#if HAVE_SE_AES
#if HAVE_CRYPTO_KCONFIG || defined(SE_AES_PREDEFINED_DYNAMIC_KEYSLOT)
/** @brief find out a dynamic SE keyslot for an AES operation.
 *
 * For SE a dynamic keyslot may be needed and the context keyslot is
 * returned if defined. If not defined, try to find a configured
 * keyslot instead.
 *
 * If the flag AES_FLAGS_DYNAMIC_KEYSLOT is set, that keyslot can be
 * freely used by CCC for all RSA operations related to this context.
 *
 * @param econtext aes engine context
 * @param dynamic_keyslot_p reference set to dynamic keyslot number for SE AES
 * @return NO_ERROR on success, error code otherwise.
 */
status_t aes_engine_get_dynamic_keyslot(const struct se_engine_aes_context *econtext,
					uint32_t *dynamic_keyslot_p);
#endif /* HAVE_CRYPTO_KCONFIG || defined(SE_AES_PREDEFINED_DYNAMIC_KEYSLOT) */

#if HAVE_SE_KAC_KEYOP

status_t se_kwuw_parse_wrap_blob(struct se_engine_aes_context *econtext,
				 const uint8_t *wb, uint32_t wb_len);

status_t se_kwuw_process_input(
	struct se_data_params *input_params,
	struct se_aes_context *aes_context);
#endif
#endif /* HAVE_SE_AES */

#if HAVE_SE_KAC_KDF
status_t se_sha_kdf_process_input(struct se_data_params *input_params,
				  struct se_sha_context *sha_context);
#endif /* HAVE_SE_KAC_KDF */

/***************************************/

/**
 * @brief Get the memory for operation key
 */
status_t context_get_key_mem(struct crypto_context_s *ctx);

/***************************************/

#if HAVE_NC_REGIONS
/**
 * @brief Get contiguous phys region length from virtual address KVA in the given
 *  domain (kernel or TA).
 *
 * Loop pages forward until the physical pages are no longer
 *  contiguous; response is the number of physically contiguous
 *  bytes starting from virtual address KVA (which may be a secure app
 *  address (32 bit TA address) or a (64 bit) Kernel address).
 *
 * Need to cast a pointer to scalar for page boundary checks and
 * bytes on page callwlations when dealing with non-contiguous memory
 * objects in physical memory when crossing page boundaries. This
 * is required if the SE engine accesses the memory via physical addresses.
 *
 * @param domain TE_CRYPTO_DOMAIN_TA for user space addresses else TE_CRYPTO_DOMAIN_KERNEL.
 * @param kva virtual address to scan forward for contiguous bytes in phys memory
 * @param remain number of bytes in kva buffer
 * @param clen_p referenced object will be set to number of contiguous bytes from kva
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t ccc_nc_region_get_length(te_crypto_domain_t domain, const uint8_t *kva,
				  uint32_t remain, uint32_t *clen_p);
#endif /* HAVE_NC_REGIONS */

#if HAVE_IS_CONTIGUOUS_MEM_FUNCTION
/**
 * @brief Check if the given buffer is contiguous across pages
 *
 * If HAVE_NC_REGIONS is 0 => this is always true.
 * If it is 1 => the buffer page boundaries verified.
 *
 * @param kva Virtual address to validate for being contiguous
 * @param size Size of kva buffer
 * @param is_contiguous_p Boolean to set when contiguous (or NULL)
 * is_contiguous_p can be left NULL if the return value is sufficient.
 *
 * @return NO_ERROR only if memory is contiguous. Also sets
 * the boolean *is_contiguous_p TRUE if the memory is contiguous.
 */
status_t se_is_contiguous_memory_region(const uint8_t *kva,
					uint32_t size,
					bool *is_contiguous_p);
#endif /* HAVE_IS_CONTIGUOUS_MEM_FUNCTION */

/***************************************/

#if HAVE_NO_STATIC_DATA_INITIALIZER

#if HAVE_SE_SHA || HAVE_SE_AES || HAVE_SE_RSA || HAVE_SE_GENRND
void ccc_static_init_se_values(void);
#endif

#if HAVE_LWPKA_RSA || HAVE_LWPKA_ECC || HAVE_LWPKA_MODULAR
void ccc_static_init_lwpka_values(void);
#endif

#endif /* HAVE_NO_STATIC_DATA_INITIALIZER */

#endif /* INCLUDED_CRYPTO_OPS_INTERN_H */
