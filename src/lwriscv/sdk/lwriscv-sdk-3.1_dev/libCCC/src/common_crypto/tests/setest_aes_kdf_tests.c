/*
 * Copyright (c) 2016-2021, LWPU CORPORATION. All rights reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <crypto_system_config.h>
#include <tests/setest_kernel_tests.h>

#if KERNEL_TEST_MODE

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#ifdef TEST_AES_KDF_TESTS
static status_t aes_kdf_key128_verification(crypto_context_t *c, uint32_t key_index, bool zero_key_test)
{
	status_t ret = NO_ERROR;

	static DMA_ALIGN_DATA uint8_t dst[DMA_ALIGN_SIZE(16)];
	static DMA_ALIGN_DATA uint8_t data[ 16 ] = { 0x00 };

	/* Correct result when key is zero */
	static const uint8_t ok_zero[] = {
		0x66, 0xE9, 0x4B, 0xD4, 0xEF, 0x8A, 0x2C, 0x3B,
		0x88, 0x4C, 0xFA, 0x59, 0xCA, 0x34, 0x2B, 0x2E,
	};

	/* Correct AES-CBC-NOPAD result when key is not zero */
	static const uint8_t ok_nonzero[] = {
		0x4a, 0x2d, 0x9a, 0x55, 0xb0, 0xd4, 0xd6, 0xf2,
		0x24, 0xf6, 0x50, 0xda, 0x62, 0x71, 0x3b, 0x96,
	};

	te_crypto_args_t arg =  { .ca_handle = 0U, };
	te_crypto_algo_t algo = TE_ALG_AES_ECB_NOPAD;

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_USE_KEYSLOT_KEY | KEY_FLAG_LEAVE_KEY_IN_KEYSLOT,
		.k_byte_size = 16U,
	};

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, algo);

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = algo;
	arg.ca_opcode	= TE_OP_COMBINED_OPERATION;

	arg.ca_init.engine_hint = CCC_ENGINE_SE0_AES1;

	akey.k_keyslot = key_index;
	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size =  sizeof_u32(data);
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	TRAP_IF_ERROR(ret, "key 128 derive cipher test operation failed: %d\n", ret);

#if LOCAL_TRACE
	{
		uint32_t written = 0U;
		written = arg.ca_data.dst_size;
		LOG_INFO("CRYPTO DST_SIZE %u, cipher with keyslot %u\n", written, key_index);

		DUMP_HEX("AES-ECB-NOPAD CIPHER RESULT:", dst, written);
	}
#endif

	if (BOOL_IS_TRUE(zero_key_test)) {
		VERIFY_ARRAY_VALUE(dst, ok_zero, sizeof_u32(ok_zero));
	} else {
		VERIFY_ARRAY_VALUE(dst, ok_nonzero, sizeof_u32(ok_nonzero));
	}

fail:
	return ret;
}

/* ECB-NOPAD encrypt zero block with 256 bit key.
 * Key is either zero key or "non-zero key" both already set up in a keyslot.
 */
static status_t aes_kdf_key256_verification(crypto_context_t *c, uint32_t key_index, bool zero_key_test)
{
	status_t ret = NO_ERROR;

	static DMA_ALIGN_DATA uint8_t dst[DMA_ALIGN_SIZE(16)];
	static DMA_ALIGN_DATA const uint8_t data[ 16 ] = { 0x00 };

	/* Correct result when 256 bit key is zero */
	static const uint8_t ok_zero[] = {
		0xdc, 0x95, 0xc0, 0x78, 0xa2, 0x40, 0x89, 0x89,
		0xad, 0x48, 0xa2, 0x14, 0x92, 0x84, 0x20, 0x87,
	};

	/* Correct AES-CBC-NOPAD result when 256 bit key is not zero */
	static const uint8_t ok_nonzero[] = {
		0xbc, 0x5e, 0x0b, 0x8b, 0x96, 0x80, 0x3c, 0x05,
		0x6a, 0xb8, 0xd3, 0x91, 0xc6, 0x9c, 0x86, 0xc8,
	};

	te_crypto_args_t arg =  { .ca_handle = 0U, };
	te_crypto_algo_t algo = TE_ALG_AES_ECB_NOPAD;

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_USE_KEYSLOT_KEY | KEY_FLAG_LEAVE_KEY_IN_KEYSLOT,
		.k_byte_size = 32U,
	};

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, algo);

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = algo;
	arg.ca_opcode	= TE_OP_COMBINED_OPERATION;

	arg.ca_init.engine_hint = CCC_ENGINE_SE0_AES1;

	akey.k_keyslot = key_index;
	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	TRAP_IF_ERROR(ret, "key 256 derive cipher test operation failed: %d\n", ret);

#if MODULE_TRACE
	uint32_t written = 0U;
	written = arg.ca_data.dst_size;
	LOG_INFO("CRYPTO DST_SIZE %u, cipher with keyslot %u\n", written, key_index);

	DUMP_HEX("AES-ECB-NOPAD CIPHER RESULT:", dst, written);
#endif

	if (BOOL_IS_TRUE(zero_key_test)) {
		VERIFY_ARRAY_VALUE(dst, ok_zero, sizeof_u32(ok_zero));
	} else {
		VERIFY_ARRAY_VALUE(dst, ok_nonzero, sizeof_u32(ok_nonzero));
	}

fail:
	return ret;
}

__STATIC__ status_t TEST_aes_key_derive_128(crypto_context_t *c, te_crypto_algo_t algo, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };
	uint32_t target_index = 3U;

	static DMA_ALIGN_DATA unsigned char data[16] = "abcdefghijklmnop";

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = 16,
		.k_aes_key = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			       0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f },
	};

	unsigned char iv[] = {
		0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
		0x88, 0x88, 0x88, 0x88, 0xc0, 0x00, 0x00, 0x00,
	};

#if 0
	// Ciphered with the above aes key and IV with AES-CTR
	uint8_t correct_key[] = {
		0x80, 0x39, 0xdf, 0xa4, 0x42, 0xd5, 0x14, 0x20,
		0x8c, 0x72, 0x64, 0x3c, 0x17, 0x9d, 0x42, 0x4d
	};
#endif

	ret = se_clear_device_aes_keyslot(SE_CDEV_SE0, target_index);
	CCC_ERROR_CHECK(ret,
			LOG_ERROR("Failed to clear aes keyslot %u\n",
				  target_index));

	// Verify that there is a zero key in the key slot
	ret = aes_kdf_key128_verification(c, target_index, true);
	TRAP_IF_ERROR(ret, "Zero key init test failed");

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_DERIVE, algo);

	if (algo != TE_ALG_AES_CTR) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LOG_ERROR("Algo 0x%x must be TE_ALG_AES_CTR for the AES CTR derivation test\n",
					       algo));
	}

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_DERIVE;
	arg.ca_algo     = algo;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg.ca_init.aes_kdf.kdf_index = target_index;
	arg.ca_init.aes_kdf.kdf_key_bit_size = 128U;
	se_util_mem_move(&arg.ca_init.aes_kdf.ctr_mode.ci_counter[0], iv, sizeof_u32(iv));

	arg.ca_init.engine_hint = eid; /* Pass the engine selector hint in init generics */

	LOG_INFO("Hint: use engine 0x%x (%s) for AES key derivation\n", eid, eid_name(eid));

	/* Pass the key params */
	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src	     = data;
#if 1
	arg.ca_data.dst_size = 0U;
	arg.ca_data.dst	     = NULL;
#else
	/* Test case for passing in a buffer which will
	 * not get modified.
	 */
	uint8_t dst[16U] = { 0x0U };

	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst	     = dst;
#endif

	ret = CRYPTO_OPERATION(c, &arg);

	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	LOG_INFO("AES-KDF[0x%x] (COMBINED OPERATION) ret %d, attached handle: %u\n",
		 algo, ret, arg.ca_handle);
	CCC_ERROR_CHECK(ret);

	LOG_INFO("Verifying AES-CTR key derivation key\n");
	ret = aes_kdf_key128_verification(c, target_index, false);
	CCC_ERROR_CHECK(ret, LOG_ERROR("Derived key check FAILED\n"));

	if (arg.ca_data.dst_size != 16U) {
		TRAP_ERROR("AES KDF result length incorrect: %u\n", arg.ca_data.dst_size);
	}

//	DUMP_HEX("dst (MUST BE ZERO)", dst, 16U);

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

__STATIC__ status_t TEST_aes_key_derive_256(crypto_context_t *c, te_crypto_algo_t algo, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };
	uint32_t target_index = 3U;

	static DMA_ALIGN_DATA unsigned char data[32] = "abcdefghijklmnop0123456789abcdef";

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = 16,
		.k_aes_key = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			       0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f },
	};

	unsigned char iv[] = {
		0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
		0x88, 0x88, 0x88, 0x88, 0xc0, 0x00, 0x00, 0x00,
	};

#if 0
	// Ciphered with the above aes key and IV with AES-CTR
	uint8_t correct_key[] = {
		0x80, 0x39, 0xdf, 0xa4, 0x42, 0xd5, 0x14, 0x20,
		0x8c, 0x72, 0x64, 0x3c, 0x17, 0x9d, 0x42, 0x4d,
		0xe1, 0x37, 0x79, 0x34, 0xdf, 0xf6, 0x95, 0x69,
		0xba, 0x3b, 0xbe, 0x85, 0x5e, 0xa1, 0x05, 0xbd,
	};
#endif

	ret = se_clear_device_aes_keyslot(SE_CDEV_SE0, target_index);
	CCC_ERROR_CHECK(ret,
			LOG_ERROR("Failed to clear aes keyslot %u\n",
				  target_index));

	// Verify that there is a zero key in the key slot
	ret = aes_kdf_key256_verification(c, target_index, true);
	TRAP_IF_ERROR(ret, "Zero key init test failed");

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_DERIVE, algo);

	if (algo != TE_ALG_AES_CTR) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LOG_ERROR("Algo 0x%x must be TE_ALG_AES_CTR for the AES CTR derivation test\n",
					       algo));
	}

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_DERIVE;
	arg.ca_algo     = algo;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg.ca_init.aes_kdf.kdf_index = target_index;
	arg.ca_init.aes_kdf.kdf_key_bit_size = 256U;
	se_util_mem_move(&arg.ca_init.aes_kdf.ctr_mode.ci_counter[0], iv, sizeof_u32(iv));

	arg.ca_init.engine_hint = eid; /* Pass the engine selector hint in init generics */

	LOG_INFO("Hint: use engine 0x%x (%s) for AES key derivation\n", eid, eid_name(eid));

	/* Pass the key params */
	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src	     = data;
#if 1
	arg.ca_data.dst_size = 0U;
	arg.ca_data.dst	     = NULL;
#else
	/* Test case for passing in a buffer which will
	 * not get modified.
	 */
	uint8_t dst[32U] = { 0x0U };

	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst	     = dst;
#endif

	ret = CRYPTO_OPERATION(c, &arg);

	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	LOG_INFO("AES-KDF[0x%x] (COMBINED OPERATION) ret %d, attached handle: %u\n",
		 algo, ret, arg.ca_handle);

	CCC_ERROR_CHECK(ret);

	if (arg.ca_data.dst_size != 32U) {
		TRAP_ERROR("AES KDF result length incorrect: %u\n", arg.ca_data.dst_size);
	}

	LOG_INFO("Verifying AES-CTR key derivation key\n");
	ret = aes_kdf_key256_verification(c, target_index, false);

	CCC_ERROR_CHECK(ret, LOG_ERROR("Derived key check FAILED\n"));

//	DUMP_HEX("dst (MUST BE ZERO)", dst, 16U);

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

__STATIC__ status_t kdf_aes_key_verify(crypto_context_t *c,
				    te_crypto_alg_mode_t mode, engine_id_t engine_id,
				    uint32_t keybytes, const uint8_t *key,
				    const uint8_t *input, const uint8_t *output,
				    uint32_t data_size, uint32_t target_index,
				    bool use_keyslot_key)
{
	status_t ret = NO_ERROR;
	static DMA_ALIGN_DATA uint8_t result[64U];
	te_crypto_args_t arg = { .ca_handle = 0U, };

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
	};

	if (NULL == c) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	CRYPTO_CONTEXT_SETUP(c, mode, TE_ALG_AES_ECB_NOPAD);

	se_util_mem_set(result, 0, sizeof_u32(result));

	arg.ca_algo     = TE_ALG_AES_ECB_NOPAD;
	arg.ca_alg_mode = mode;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;
	arg.ca_init.engine_hint = engine_id;

	akey.k_byte_size = keybytes;
	akey.k_keyslot = target_index;

	if (BOOL_IS_TRUE(use_keyslot_key)) {
		akey.k_flags = KEY_FLAG_USE_KEYSLOT_KEY | KEY_FLAG_LEAVE_KEY_IN_KEYSLOT;
	} else if (NULL != key) {
		se_util_mem_move(akey.k_aes_key, key, akey.k_byte_size);
	} else {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				LOG_ERROR("key arguments are invalid.\n"));

	}

	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = data_size;
	arg.ca_data.src = input;
	arg.ca_data.dst_size = data_size;
	arg.ca_data.dst = result;

	ret = CRYPTO_OPERATION(c, &arg);
	CCC_ERROR_CHECK(ret);

	if (0U != memory_compare(result, output, data_size)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				LOG_ERROR("result compare failed; ALGO: 0x%x, MODE: %u\n",
					TE_ALG_AES_ECB_NOPAD, mode));
	}

fail:
	CRYPTO_CONTEXT_RESET(c);

	return ret;
}

__STATIC__ status_t aes_kdf_mode_128(crypto_context_t *c,
		te_crypto_algo_t algo,
		engine_id_t eid,
		const uint8_t *iv,
		const uint8_t *secret_value128,
		const uint8_t *derived_key128,
		const uint8_t *kdf_key_128,
		const uint8_t *key_verify_plaintext,
		uint32_t pt_size,
		const uint8_t *key128_verify_ciphertext,
		uint32_t ct_size,
		bool overwrite_derive_key)
{
	status_t ret = NO_ERROR;
	uint32_t target_index = 3U;
	uint32_t kdf_target_byte_size = 128U/8U;
	const uint8_t *secret_value;
	const uint8_t *key_verify_ciphertext;
	uint32_t secret_value_size;

	te_crypto_args_t arg = { .ca_handle = 0U, };

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,  // AES_KEYSLOT < 15
		.k_byte_size = 16U,
	};

	/* Check if destination keyslot and derive keyslot are the same */
	if (BOOL_IS_TRUE(overwrite_derive_key)) {
		target_index = akey.k_keyslot;
		akey.k_flags |= KEY_FLAG_LEAVE_KEY_IN_KEYSLOT;
	} else {
		target_index = akey.k_keyslot + 1U;
	}

	ret = se_clear_device_aes_keyslot(SE_CDEV_SE0, target_index);
	CCC_ERROR_CHECK(ret, LOG_ERROR("Failed to clear aes keyslot %u\n",
				  target_index));

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_DERIVE, algo);

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_DERIVE;
	arg.ca_algo     = algo;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg.ca_init.aes_kdf.kdf_index = target_index;
	arg.ca_init.aes_kdf.kdf_key_bit_size = 128U;
	arg.ca_init.engine_hint = eid; /* Pass the engine selector hint in init generics */

	if (NULL != iv) {
		se_util_mem_move(&arg.ca_init.aes_kdf.ci_iv[0], iv, 16U);
	}

	if (16U == kdf_target_byte_size) {
		se_util_mem_move(akey.k_aes_key, kdf_key_128, akey.k_byte_size);
		secret_value = &secret_value128[0];
		secret_value_size = 16U;
		key_verify_ciphertext = &key128_verify_ciphertext[0];
	}

	LOG_INFO("Hint: use engine 0x%x (%s) for AES key derivation\n", eid, eid_name(eid));

	/* Pass the key params */
	arg.ca_set_key.kdata = &akey;
	arg.ca_data.src_size = secret_value_size;
	arg.ca_data.src	     = secret_value;
	arg.ca_data.dst_size = 0U;
	arg.ca_data.dst	     = NULL;

	ret = CRYPTO_OPERATION(c, &arg);

	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	LOG_INFO("AES-KDF[0x%x] (COMBINED OPERATION) ret %d, attached handle: %u\n",
		 algo, ret, arg.ca_handle);
	CCC_ERROR_CHECK(ret);

	CRYPTO_CONTEXT_RESET(c);

	/* Verify the derived AES key in target AES keyslot */
	ret = kdf_aes_key_verify(c, TE_ALG_MODE_ENCRYPT, CCC_ENGINE_SE0_AES1,
			16U, NULL, key_verify_plaintext, key_verify_ciphertext,
			16U, target_index, true);
	CCC_ERROR_CHECK(ret);

	/* Verify the derived AES key from argument */
	ret = kdf_aes_key_verify(c, TE_ALG_MODE_ENCRYPT, CCC_ENGINE_SE0_AES1,
			16U, derived_key128, key_verify_plaintext, key_verify_ciphertext,
			16U, target_index, false);
	CCC_ERROR_CHECK(ret);

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

__STATIC__ status_t TEST_aes_kdf_ecb_128(crypto_context_t *c,
		te_crypto_algo_t algo, engine_id_t eid, bool overwrite_drv_key)
{
	status_t ret = NO_ERROR;

	/* Secret value as input to AES-ECB KDF to derive 128-bit secret key
	 * Derived AES key: 6bc1bee22e409f96e93d7e117393172a
	 */
	DMA_ALIGN_DATA const uint8_t secret_value128[] = {
		0x3a, 0xd7, 0x7b, 0xb4, 0x0d, 0x7a, 0x36, 0x60,
		0xa8, 0x9e, 0xca, 0xf3, 0x24, 0x66, 0xef, 0x97,
	};

	const uint8_t derived_key128[] = {
		0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
		0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
	};

	const uint8_t ecb_derive_key_128[] = {
		0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
		0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
	};

	const uint8_t key_verify_plaintext[16] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	};

	const uint8_t key128_verify_ciphertext[16] = {
		0x12, 0x95, 0x5b, 0xc3, 0xdd, 0xe2, 0x4c, 0xbc,
		0x31, 0x99, 0x9a, 0x11, 0x8b, 0x5f, 0x13, 0x27,
	};

	ret = aes_kdf_mode_128(c, algo, eid, NULL, &secret_value128[0],
			&derived_key128[0], &ecb_derive_key_128[0],
			&key_verify_plaintext[0],
			sizeof_u32(key_verify_plaintext),
			&key128_verify_ciphertext[0],
			sizeof_u32(key128_verify_ciphertext),
			overwrite_drv_key);
	CCC_ERROR_CHECK(ret);

fail:
	return ret;
}

__STATIC__ status_t TEST_aes_kdf_cbc_128(crypto_context_t *c,
		te_crypto_algo_t algo, engine_id_t eid, bool overwrite_drv_key)
{
	status_t ret = NO_ERROR;

	const uint8_t iv[] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	};

	/* Secret value as input to AES-CBC KDF to derive 128-bit secret key
	 * Derived AES key: 6bc1bee22e409f96e93d7e117393172a
	 */
	DMA_ALIGN_DATA const uint8_t secret_value128[] = {
		0x76, 0x49, 0xab, 0xac, 0x81, 0x19, 0xb2, 0x46,
		0xce, 0xe9, 0x8e, 0x9b, 0x12, 0xe9, 0x19, 0x7d,
	};

	const uint8_t derived_key128[] = {
		0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
		0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
	};

	const uint8_t cbc_kdf_key_128[] = {
		0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
		0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
	};

	const uint8_t key_verify_plaintext[16] = {
		0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
		0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
	};

	const uint8_t key128_verify_ciphertext[16] = {
		0xe4, 0x66, 0x47, 0x5d, 0x6c, 0x16, 0xd5, 0x4f,
		0xe9, 0xdf, 0x3b, 0x79, 0xa2, 0x47, 0xc2, 0x62,
	};

	ret = aes_kdf_mode_128(c, algo, eid, &iv[0], &secret_value128[0],
			&derived_key128[0], &cbc_kdf_key_128[0],
			&key_verify_plaintext[0],
			sizeof_u32(key_verify_plaintext),
			&key128_verify_ciphertext[0],
			sizeof_u32(key128_verify_ciphertext),
			overwrite_drv_key);
	CCC_ERROR_CHECK(ret);

fail:
	return ret;
}

status_t run_aes_kdf_test_cases(crypto_context_t *crypto_ctx)
{
	status_t ret = NO_ERROR;

	/* AES-CTR mode key derive and result verification */
#if CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T18X)
	TEST_EXPECT_RET(TEST_aes_key_derive_128, ERR_NOT_ALLOWED, CCC_ENGINE_ANY, TE_ALG_AES_CTR);
	TEST_EXPECT_RET(TEST_aes_key_derive_256, ERR_NOT_ALLOWED, CCC_ENGINE_ANY, TE_ALG_AES_CTR);
#else
	TEST_ERRCHK(TEST_aes_key_derive_128, CCC_ENGINE_ANY, TE_ALG_AES_CTR);
	TEST_ERRCHK(TEST_aes_key_derive_256, CCC_ENGINE_ANY, TE_ALG_AES_CTR);
#endif
	TEST_ERRCHK(TEST_aes_kdf_ecb_128, CCC_ENGINE_ANY, TE_ALG_AES_ECB_NOPAD, false);
	TEST_ERRCHK(TEST_aes_kdf_cbc_128, CCC_ENGINE_ANY, TE_ALG_AES_CBC_NOPAD, false);
	TEST_ERRCHK(TEST_aes_kdf_ecb_128, CCC_ENGINE_ANY, TE_ALG_AES_ECB_NOPAD, true);
	TEST_ERRCHK(TEST_aes_kdf_cbc_128, CCC_ENGINE_ANY, TE_ALG_AES_CBC_NOPAD, true);

fail:
	return ret;
}
#endif /* TEST_AES_KDF_TESTS */
#endif /* KERNEL_TEST_MODE */
