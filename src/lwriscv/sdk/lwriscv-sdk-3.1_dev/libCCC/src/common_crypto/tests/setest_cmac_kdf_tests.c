/*
 * Copyright (c) 2020-2021, LWPU CORPORATION. All rights reserved
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

#define TEST_VERIFY_KEY_SETUP

#define AES_KEYSLOT 2U

#ifdef TEST_CMAC_KDF_TESTS
static status_t cmac_kdf_key128_verification(crypto_context_t *c, uint32_t key_index, bool zero_key_test)
{
	status_t ret = NO_ERROR;

	static DMA_ALIGN_DATA uint8_t dst[DMA_ALIGN_SIZE(16)];
	static DMA_ALIGN_DATA uint8_t data[ 16 ] = { 0x00 };

	/* Correct AES-ECB kdf result when key is zero */
	static const uint8_t ok_zero[] = {
		0x66, 0xE9, 0x4B, 0xD4, 0xEF, 0x8A, 0x2C, 0x3B,
		0x88, 0x4C, 0xFA, 0x59, 0xCA, 0x34, 0x2B, 0x2E,
	};

	/* Correct AES-ECB-NOPAD result when key is not zero
	 *
	 * CMAC(key:000102030405060708090a0b0c0d0e0f) of
	 * 000000016C6162656C00636F6E7465787400000080 is
	 * 0e0ef025aa00b4a767fbf6eb3ce2261c
	 *
	 * AES-ECB-NOPAD(K:0e0ef025aa00b4a767fbf6eb3ce2261c) of
	 * zero block is 9e8b95150f74eaac55fb71be9b8f772a
	 */
	static const uint8_t ok_nonzero[] = {
		0x9e, 0x8b, 0x95, 0x15, 0x0f, 0x74, 0xea, 0xac,
		0x55, 0xfb, 0x71, 0xbe, 0x9b, 0x8f, 0x77, 0x2a,
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
static status_t cmac_kdf_key256_verification(crypto_context_t *c, uint32_t key_index, bool zero_key_test)
{
	status_t ret = NO_ERROR;

	static DMA_ALIGN_DATA uint8_t dst[DMA_ALIGN_SIZE(16)];
	static DMA_ALIGN_DATA const uint8_t data[ 16 ] = { 0x00 };

	/* Correct result when 256 bit key is zero */
	static const uint8_t ok_zero[] = {
		0xdc, 0x95, 0xc0, 0x78, 0xa2, 0x40, 0x89, 0x89,
		0xad, 0x48, 0xa2, 0x14, 0x92, 0x84, 0x20, 0x87,
	};

	/* Correct AES-CBC-NOPAD result when 256 bit key is not zero
	 *
	 * CMAC(key:000102030405060708090a0b0c0d0e0f) of
	 * 000000016C6162656C00636F6E7465787400000100 is
	 * 4a63599f94bd3bf38b763386bb5397c6
	 *
	 * CMAC(key:000102030405060708090a0b0c0d0e0f) of
	 * 000000026C6162656C00636F6E7465787400000100 is
	 * is 90a3a40600f96a691059cc001c163c49 (UPPER QUAD)
	 *
	 * AES-ECB-NOPAD-256(K:4a63599f94bd3bf38b763386bb5397c690a3a40600f96a691059cc001c163c49)
	 * of zeroblock is ce65209dc6cfc57921401a31ea98fde3
	 */
	static const uint8_t ok_nonzero[] = {
		0xd0, 0x0b, 0x47, 0xa7, 0x43, 0x93, 0xf7, 0x43,
		0x7a, 0xca, 0x69, 0x27, 0xb5, 0x06, 0xd2, 0xf2,
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

static status_t TEST_cmac_key_nist_derive_128(crypto_context_t *c, te_crypto_algo_t algo, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };
	uint32_t target_index = 3U;

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = 16,
		.k_aes_key = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			       0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f },
	};

	const uint8_t label[]   = { 0x6c, 0x61, 0x62, 0x65, 0x6c }; // "label"
	const uint8_t context[] = { 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x78, 0x74 }; // "context"

#ifdef TEST_VERIFY_KEY_SETUP
	ret = se_clear_device_aes_keyslot(SE_CDEV_SE0, target_index);
	CCC_ERROR_CHECK(ret,
			LOG_ERROR("Failed to clear aes keyslot %u\n",
				  target_index));

	// Verify that there is a zero key in the key slot
	ret = cmac_kdf_key128_verification(c, target_index, true);
	TRAP_IF_ERROR(ret, "Zero key init test failed");
#endif

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_DERIVE, algo);

	if (algo != TE_ALG_CMAC_AES) {
		LOG_ERROR("Algo 0x%x must be TE_ALG_CMAC_AES for the CMAC derivation test\n", algo);
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_DERIVE;
	arg.ca_algo     = algo;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg.ca_init.cmac_kdf.kdf_index = target_index;
	arg.ca_init.cmac_kdf.kdf_key_bit_size = 128U;

	arg.ca_init.engine_hint = eid; /* Pass the engine selector hint in init generics */

	LOG_INFO("Hint: use engine 0x%x (%s) for AES key derivation\n", eid, eid_name(eid));

	/* Pass the key params */
	arg.ca_set_key.kdata = &akey;

	/* Set LABEL field */
	arg.ca_data.src_kdf_label_len = sizeof_u32(label);
	arg.ca_data.src_kdf_label     = label;

	/* Set CONTEXT field */
	arg.ca_data.src_kdf_context_len = sizeof_u32(context);
	arg.ca_data.src_kdf_context     = context;

	ret = CRYPTO_OPERATION(c, &arg);

	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	LOG_INFO("CMAC kdf[0x%x] (COMBINED OPERATION) ret %d\n", algo, ret);
	CCC_ERROR_CHECK(ret);

	LOG_INFO("Verifying CMAC derived key\n");

	ret = cmac_kdf_key128_verification(c, target_index, false);
	CCC_ERROR_CHECK(ret, LOG_ERROR("Derived key check FAILED\n"));

	if (arg.ca_data.dst_size != 16U) {
		TRAP_ERROR("CMAC KDF result length incorrect: %u\n", arg.ca_data.dst_size);
	}
fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

static status_t TEST_cmac_key_nist_derive_256(crypto_context_t *c, te_crypto_algo_t algo, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };
	uint32_t target_index = 3U;

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = 16,
		.k_aes_key = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			       0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f },
	};

	const uint8_t label[]   = { 0x6c, 0x61, 0x62, 0x65, 0x6c }; // "label"
	const uint8_t context[] = { 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x78, 0x74 }; // "context"

#ifdef TEST_VERIFY_KEY_SETUP
	ret = se_clear_device_aes_keyslot(SE_CDEV_SE0, target_index);
	CCC_ERROR_CHECK(ret,
			LOG_ERROR("Failed to clear aes keyslot %u\n",
				  target_index));

	// Verify that there is a zero key in the key slot
	ret = cmac_kdf_key256_verification(c, target_index, true);
	TRAP_IF_ERROR(ret, "Zero key init test failed");
#endif

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_DERIVE, algo);

	if (algo != TE_ALG_CMAC_AES) {
		LOG_ERROR("Algo 0x%x must be TE_ALG_CMAC_AES for the CMAC kdf test\n", algo);
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_DERIVE;
	arg.ca_algo     = algo;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg.ca_init.aes_kdf.kdf_index = target_index;
	arg.ca_init.aes_kdf.kdf_key_bit_size = 256U;

	arg.ca_init.engine_hint = eid; /* Pass the engine selector hint in init generics */

	LOG_INFO("Hint: use engine 0x%x (%s) for AES key derivation\n", eid, eid_name(eid));

	/* Pass the key params */
	arg.ca_set_key.kdata = &akey;

	/* Set LABEL field */
	arg.ca_data.src_kdf_label_len = sizeof_u32(label);
	arg.ca_data.src_kdf_label     = label;

	/* Set CONTEXT field */
	arg.ca_data.src_kdf_context_len = sizeof_u32(context);
	arg.ca_data.src_kdf_context     = context;

	ret = CRYPTO_OPERATION(c, &arg);

	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	LOG_INFO("CMAC KDF[0x%x] (COMBINED OPERATION) ret %d\n", algo, ret);

	CCC_ERROR_CHECK(ret);

	if (arg.ca_data.dst_size != 32U) {
		TRAP_ERROR("AES KDF result length incorrect: %u\n", arg.ca_data.dst_size);
	}

	LOG_INFO("Verifying CMAC KDF derived key\n");
	ret = cmac_kdf_key256_verification(c, target_index, false);

	CCC_ERROR_CHECK(ret, LOG_ERROR("Derived key check FAILED\n"));

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

status_t run_cmac_kdf_test_cases(crypto_context_t *crypto_ctx)
{
	status_t ret = NO_ERROR;

	/* CMAC mode key derive and result verification */
#if !CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T19X)
	TEST_EXPECT_RET(TEST_cmac_key_nist_derive_128, ERR_NOT_ALLOWED, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);
	TEST_EXPECT_RET(TEST_cmac_key_nist_derive_256, ERR_NOT_ALLOWED, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);
#else
	TEST_ERRCHK(TEST_cmac_key_nist_derive_128, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);
	TEST_ERRCHK(TEST_cmac_key_nist_derive_256, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);
#endif

fail:
	return ret;
}
#endif /* TEST_CMAC_KDF_TESTS */
#endif /* KERNEL_TEST_MODE */
