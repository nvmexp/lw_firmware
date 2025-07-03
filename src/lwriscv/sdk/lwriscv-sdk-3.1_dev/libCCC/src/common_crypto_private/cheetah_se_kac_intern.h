/*
 * Copyright (c) 2018-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/* Internal include only for T23X files.
 * Exports some extra APIs for tegra_se_kac_keyop.c
 *
 * To be included after tega_se_kac.h (included by tegra_se.h)
 */
#ifndef INCLUDED_TEGRA_SE_KAC_INTERN_H
#define INCLUDED_TEGRA_SE_KAC_INTERN_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

static inline bool m_is_alg_key_wrapping(te_crypto_algo_t algo)
{
	return ((TE_ALG_KEYOP_KW == algo) ||
		(TE_ALG_KEYOP_KUW == algo) ||
		(TE_ALG_KEYOP_KWUW == algo));
}

#define IS_ALG_KEY_WRAPPING(alg) m_is_alg_key_wrapping(alg)

#define IS_ALG_GENKEY(alg) \
	(TE_ALG_KEYOP_GENKEY == (alg))

static inline bool m_is_alg_kdf_key_derivation(te_crypto_algo_t algo)
{
	return ((TE_ALG_KEYOP_KDF_1KEY == algo) ||
		(TE_ALG_KEYOP_KDF_2KEY == algo));
}

#define IS_ALG_KDF_KEY_DERIVATION(alg) m_is_alg_kdf_key_derivation(alg)

static inline bool m_aes_mac_algo(te_crypto_algo_t algo)
{
	return ((TE_ALG_CMAC_AES == algo) ||
		(TE_ALG_HMAC_SHA1 == algo) ||
		(TE_ALG_HMAC_SHA224 == algo) ||
		(TE_ALG_HMAC_SHA256 == algo) ||
		(TE_ALG_HMAC_SHA384 == algo) ||
		(TE_ALG_HMAC_SHA512 == algo));
}

#define AES_MAC_ALGO(alg) m_aes_mac_algo(alg)

status_t se_write_keybuf(const engine_t *engine, uint32_t keyslot,
			 const uint8_t *ksdata, uint32_t key_words_arg);

status_t se_set_engine_kac_keyslot_locked(const engine_t *engine,
					  uint32_t keyslot,
					  const uint8_t *keydata,
					  uint32_t keysize_bits,
					  uint32_t se_kac_user,
					  uint32_t se_kac_flags,
					  uint32_t se_kac_sw,
					  te_crypto_algo_t se_kac_algorithm);

status_t se_set_eng_kac_keyslot_purpose_locked(const engine_t *engine,
					       uint32_t keyslot,
					       const uint8_t *keydata,
					       uint32_t keysize_bits,
					       uint32_t se_kac_user,
					       uint32_t se_kac_flags,
					       uint32_t se_kac_sw,
					       uint32_t se_kac_purpose);

#if HAVE_SE_KAC_GENKEY
status_t se_engine_kac_keyslot_genkey_locked(const engine_t *engine,
					     uint32_t keyslot,
					     uint32_t keysize_bits,
					     uint32_t se_kac_user,
					     uint32_t se_kac_flags,
					     uint32_t se_kac_sw,
					     te_crypto_algo_t se_kac_algorithm);
#endif

#if HAVE_SE_KAC_LOCK
status_t se_engine_kac_keyslot_lock_locked(const engine_t *engine,
					   uint32_t keyslot);
#endif

#if HAVE_SE_KAC_CLONE
status_t se_engine_kac_keyslot_clone_locked(const engine_t *engine,
					    uint32_t dst_keyslot,
					    uint32_t src_keyslot,
					    uint32_t clone_user,
					    uint32_t clone_sw,
					    uint32_t clone_kac_flags);
#endif

status_t kac_setup_keyslot_data_locked(
	const engine_t *engine,
	const struct se_key_slot *kslot,
	bool write_keybuf);

status_t engine_aes_set_block_count(
	const struct se_engine_aes_context *econtext,
	uint32_t num_bytes,
	uint32_t *num_blocks_p);

status_t setup_aes_op_mode_locked(
	const struct se_engine_aes_context *econtext, te_crypto_algo_t algorithm,
	bool is_encrypt, bool use_orig_iv, uint32_t config_dst);

status_t engine_aes_op_mode_normal(const struct se_engine_aes_context *econtext,
				   te_crypto_algo_t algorithm,
				   bool is_encrypt, bool use_orig_iv, uint32_t pkt_mode,
				   uint32_t config_dst);

status_t aes_encrypt_decrypt_arg_check(const async_aes_ctx_t *ac);

status_t engine_aes_setup_key_iv(async_aes_ctx_t *ac);

status_t aes_input_output_regs(async_aes_ctx_t *ac);

status_t aes_process_locked_async_start(async_aes_ctx_t *ac);

status_t aes_process_locked_async_finish(async_aes_ctx_t *ac);

#endif /* INCLUDED_TEGRA_SE_KAC_INTERN_H */
