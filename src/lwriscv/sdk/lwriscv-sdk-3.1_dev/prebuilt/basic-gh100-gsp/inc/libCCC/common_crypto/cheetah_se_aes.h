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

#ifndef INCLUDED_TEGRA_SE_AES_H
#define INCLUDED_TEGRA_SE_AES_H

#if HAVE_SE_UNWRAP || HAVE_CMAC_DST_KEYTABLE || HAVE_SE_AES_KDF

/* Support function to set the fields of CRYPTO_KEYTABLE_DST
 * register for keyslot target operations (unwrap, derive).
 */
status_t aes_keytable_dst_reg_setup(const struct se_engine_aes_context *econtext,
				    bool upper_quad);
#endif /* HAVE_SE_UNWRAP || HAVE_CMAC_DST_KEYTABLE || HAVE_SE_AES_KDF */

status_t setup_aes_op_mode_locked(
	const struct se_engine_aes_context *econtext,
	te_crypto_algo_t algorithm,
	bool is_encrypt,
	bool use_orig_iv,
	uint32_t config_dst);

status_t engine_aes_set_block_count(
	const struct se_engine_aes_context *econtext,
	uint32_t num_bytes,
	uint32_t *num_blocks_p);

status_t aes_key_wordsize(uint32_t keysize_bits, uint32_t *key_wordsize_p);

status_t aes_read_iv_locked(
	const engine_t *engine,
	uint32_t keyslot,
	se_aes_keyslot_id_sel_t id_sel, uint32_t *keydata_param);

#endif /* #define INCLUDED_TEGRA_SE_AES_H */
