/*
 * Copyright (c) 2018-2021, LWPU CORPORATION. All rights reserved
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

//#define CCC_TRACE_THIS_FILE

#include <crypto_system_config.h>
#include <tests/setest_kernel_tests.h>

/* Defined nonzero to run the kernel mode crypto tests
 */
#if KERNEL_TEST_MODE && CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T23X)

#define AES_KEYSLOT 12U
#define HMAC_KEYSLOT 5U

#include <crypto_api.h>
#include <crypto_ops.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

status_t run_psc_tests(crypto_context_t *crypto_ctx, engine_id_t engine_id);

#if !defined(HAVE_HW_HMAC_SHA) || (HAVE_HW_HMAC_SHA == 0)
#define PSC_CIPHER_DECIPHER_TESTS 1
#else
// Out of memory => disable these tests for now
#define PSC_CIPHER_DECIPHER_TESTS 0
#endif

#if PSC_CIPHER_DECIPHER_TESTS

/* This test sets the AES KEY to keyslot 2 and leaves it there after the operation completes.
 *
 * It will be used by the next decipher test which does not know what the key is supposed to be.
 */
static status_t TEST_psc_cipher_decipher(crypto_context_t *c, te_crypto_algo_t aes_mode, engine_id_t eid,
					 uint32_t keysize_bits)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	unsigned char data[16] = "abcdefghijklmnop";
	const uint8_t *ctext = NULL;

	uint32_t key_flags = KEY_FLAG_PLAIN | KEY_FLAG_LEAVE_KEY_IN_KEYSLOT;

	DMA_ALIGN_DATA unsigned char dst[16];
	uint32_t written = 0;
	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = 16,	/* 128 bit key default */
	};

	/* 128 (16 bytes), 192 (24 bytes) and 256 (32 bytes) AES keys for the operations.
	 * The order is like this to make sure the "prefix bytes of key" always change when
	 * switching to a longer key.
	 */
	const uint8_t aes_key256_192_128[] = { /* 256 bit key follows: Offset 0 */
					       0xDE, 0xAD, 0xC0, 0xDE, 0xFF, 0xFF, 0xFF, 0xFF,
					       /* 192 bit key follows: Offset 8 */
					       0xBA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAD,
					       /* 128 bit key follows: Offset 16 */
					       0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
					       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	const uint8_t cip_128[] = { 0xa3, 0x1e, 0x9c, 0x73, 0x12, 0xd2, 0xcd, 0x2b, 0x67, 0xd5, 0x8f, 0x08, 0x69, 0x7a, 0xd2, 0x33, };
	const uint8_t cip_192[] = { 0x21, 0x6b, 0x1a, 0x83, 0x91, 0x48, 0x16, 0xba, 0xac, 0xd2, 0x79, 0x77, 0xcb, 0xa0, 0xe3, 0x2b, };
	const uint8_t cip_256[] = { 0x9b, 0x72, 0xc8, 0x52, 0x5e, 0x82, 0x15, 0x44, 0x0d, 0xc2, 0x82, 0x55, 0x01, 0x35, 0x9e, 0xf4, };

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, aes_mode);

	se_util_mem_set(dst, 0, sizeof_u32(dst));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = aes_mode;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	(void)se_util_mem_set(arg.ca_init.aes.ci_iv, 0, 16);

	if (aes_mode == TE_ALG_AES_CTR) {
		arg.ca_init.aes.ctr_mode.ci_increment = 1;
		// Constant CTR mode preset value is a security issue if key is constant
		// but hey, this is just a test...
		se_util_mem_set(arg.ca_init.aes.ctr_mode.ci_counter, 0, 16);
	}

	arg.ca_opcode = TE_OP_INIT;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	LOG_INFO("CRYPTO(init): ALGO: 0x%x, ret %d (sizeof arg %u)\n",
		 aes_mode, ret, sizeof_u32(arg));
	CCC_ERROR_CHECK(ret);

	akey.k_flags = key_flags;

	switch (keysize_bits) {
	case 128U:
		akey.k_byte_size = 16U;
		ctext = &cip_128[0];
		break;
	case 192U:
		akey.k_byte_size = 24U;
		ctext = &cip_192[0];
		break;
	case 256U:
		akey.k_byte_size = 32U;
		ctext = &cip_256[0];
		break;
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	se_util_mem_move(&akey.k_aes_key[0],
			 &aes_key256_192_128[32U - akey.k_byte_size], akey.k_byte_size);

	arg.ca_set_key.kdata = &akey;
	arg.ca_opcode = TE_OP_SET_KEY;

	LOG_HEXBUF("XXX AES key =>",  akey.k_aes_key, akey.k_byte_size);

	ret = CRYPTO_OPERATION(c, &arg);

	LOG_INFO("CRYPTO(set_key) handle %u: ret %d\n",
		   arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	arg.ca_opcode = TE_OP_DOFINAL;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	LOG_INFO("AES-%u encrypt\n", akey.k_byte_size * 8U);

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("CRYPTO(dofinal) handle %u: ret %d\n",
		 arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("AES CIPHER FAILED\n"));

	written = arg.ca_data.dst_size;
	LOG_INFO("CRYPTO(after dofinal): DST_SIZE %u\n", written);

	DUMP_HEX("PLAINTEXT:", data, sizeof_u32(data));
	DUMP_HEX("CIPHER RESULT:", dst, sizeof_u32(dst));

	VERIFY_ARRAY_VALUE(ctext, dst, sizeof_u32(data));
	LOG_INFO("CIPHERTEXT verified correct with AES-%d\n", keysize_bits);

	CRYPTO_CONTEXT_RESET(c);

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_DECRYPT, aes_mode);

	/* decipher hte previous result and compare with original */

	LOG_INFO("Decipher data ciphered above and compare that the result matches original data\n");

	se_util_mem_set((uint8_t *)&arg, 0, sizeof_u32(arg));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_DECRYPT;
	arg.ca_algo     = aes_mode;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	se_util_mem_set(arg.ca_init.aes.ci_iv, 0, 16);

	if (aes_mode == TE_ALG_AES_CTR) {
		// Use default: counter 0 maps to 1
		// arg.ca_init.aes.ctr_mode.ci_increment = 1;

		// Constant CTR mode preset value is a security issue if key is constant
		// but hey, this is just a test...
		se_util_mem_set(arg.ca_init.aes.ctr_mode.ci_counter, 0, 16);
	}

	arg.ca_set_key.kdata = &akey;

	// XXXX Clear the key to make sure if can not be restored to keyslot
	se_util_mem_set(akey.k_aes_key, 0x0, akey.k_byte_size);

	/* Use the key left to keyslot for this decipher operation */
	akey.k_flags |= KEY_FLAG_USE_KEYSLOT_KEY;

	LOG_INFO("AES-%u decipher, using keyslot key\n", akey.k_byte_size * 8U);

	DUMP_HEX("CIPHERTEXT (to decipher):", dst, sizeof_u32(dst));

	/* Decipher in place */
	arg.ca_data.src_size = written;
	arg.ca_data.src = dst;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	LOG_INFO("CRYPTO (decipher in place): SRC len %u, ALGO: 0x%x, ret %d, attached handle: %u\n",
		 written, aes_mode, ret, arg.ca_handle);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("AES DECIPHER FAILED\n"));

	DUMP_HEX("DECIPHER RESULT:", dst, sizeof_u32(dst));

	VERIFY_ARRAY_VALUE(dst, data, sizeof_u32(data));

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

static status_t TEST_psc_decipher_fixed_key(crypto_context_t *c, te_crypto_algo_t aes_mode, engine_id_t eid,
					    uint32_t keysize_bits)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	const unsigned char correct_data[16] = "abcdefghijklmnop";

	const uint8_t cip_128[] = { 0xa3, 0x1e, 0x9c, 0x73, 0x12, 0xd2, 0xcd, 0x2b, 0x67, 0xd5, 0x8f, 0x08, 0x69, 0x7a, 0xd2, 0x33, };
	const uint8_t cip_192[] = { 0x21, 0x6b, 0x1a, 0x83, 0x91, 0x48, 0x16, 0xba, 0xac, 0xd2, 0x79, 0x77, 0xcb, 0xa0, 0xe3, 0x2b, };
	const uint8_t cip_256[] = { 0x9b, 0x72, 0xc8, 0x52, 0x5e, 0x82, 0x15, 0x44, 0x0d, 0xc2, 0x82, 0x55, 0x01, 0x35, 0x9e, 0xf4, };

	DMA_ALIGN_DATA unsigned char dst[16];
	const uint8_t *src_ciphertext = NULL;

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN | KEY_FLAG_LEAVE_KEY_IN_KEYSLOT | KEY_FLAG_USE_KEYSLOT_KEY,
		.k_keyslot = AES_KEYSLOT,
		// Key value is not know, using existing key, key size in parameters
	};

	if (aes_mode != TE_ALG_AES_CBC_NOPAD) {
		CCC_ERROR_WITH_ECODE((ERR_NOT_IMPLEMENTED),
				     CCC_ERROR_MESSAGE("Only AES-CBC-NOPAD data exists lwrrently\n"));
	}

	switch (keysize_bits) {
	case 128U:
		src_ciphertext = cip_128;
		akey.k_byte_size = 16U;
		break;
	case 192U:
		src_ciphertext = cip_192;
		akey.k_byte_size = 24U;
		break;
	case 256U:
		src_ciphertext = cip_256;
		akey.k_byte_size = 32U;
		break;
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	DUMP_HEX("CIPHER TEXT:", src_ciphertext, 16U);

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_DECRYPT, aes_mode);

	se_util_mem_set(dst, 0, sizeof_u32(dst));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_DECRYPT;
	arg.ca_algo     = aes_mode;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	(void)se_util_mem_set(arg.ca_init.aes.ci_iv, 0, 16);

	if (aes_mode == TE_ALG_AES_CTR) {
		// Use default: counter 0 maps to 1
		// arg.ca_init.aes.ctr_mode.ci_increment = 1;

		// Constant CTR mode preset value is a security issue if key is constant
		// but hey, this is just a test...
		se_util_mem_set(arg.ca_init.aes.ctr_mode.ci_counter, 0, 16);
	}

	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = 16U;
	arg.ca_data.src = src_ciphertext;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("AES DECIPHER FAILED\n"));

	DUMP_HEX("DECIPHER RESULT:", dst, sizeof_u32(dst));

	VERIFY_ARRAY_VALUE(dst, correct_data, sizeof_u32(correct_data));

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}
#endif /* PSC_CIPHER_DECIPHER_TESTS */

#if HAVE_SE_KAC_KEYOP

static status_t keyop_kwuw_key_verification(crypto_context_t *c, uint32_t key_index, bool zero_key_test)
{
	status_t ret = NO_ERROR;

	DMA_ALIGN_DATA uint8_t dst[DMA_ALIGN_SIZE(16)];
	DMA_ALIGN_DATA uint8_t data[ 16 ] = { 0x00 };

	/* Correct result when key is zero */
	static const uint8_t ok_zero[] = {
		0x66, 0xE9, 0x4B, 0xD4, 0xEF, 0x8A, 0x2C, 0x3B,
		0x88, 0x4C, 0xFA, 0x59, 0xCA, 0x34, 0x2B, 0x2E,
	};

	/* Correct result when key is not zero */
	static const uint8_t ok_nonzero[] = {
		0xC6, 0xA1, 0x3B, 0x37, 0x87, 0x8F, 0x5B, 0x82,
		0x6F, 0x4F, 0x81, 0x62, 0xA1, 0xC8, 0xD8, 0x79,
	};

	te_crypto_args_t arg =  { .ca_handle = 0U, };
	te_crypto_algo_t algo = TE_ALG_AES_ECB_NOPAD;

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_USE_KEYSLOT_KEY | KEY_FLAG_LEAVE_KEY_IN_KEYSLOT,
		.k_byte_size = 16U,
	};

	uint32_t data_len = sizeof_u32(data);

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, algo);

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = algo;
	arg.ca_opcode	= TE_OP_COMBINED_OPERATION;

	arg.ca_init.engine_hint = CCC_ENGINE_SE0_AES1;

	akey.k_keyslot = key_index;
	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = data_len;
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	TRAP_IF_ERROR(ret, "key wrap cipher test operation failed: %d\n", ret);

#if MODULE_TRACE
	uint32_t written = 0U;
	written = arg.ca_data.dst_size;
	LOG_INFO("CRYPTO DST_SIZE %u, cipher with keyslot %u\n", written, key_index);
#endif

	DUMP_HEX("KWUW CIPHER RESULT:", dst, written);

	if (BOOL_IS_TRUE(zero_key_test)) {
		VERIFY_ARRAY_VALUE(dst, ok_zero, sizeof_u32(ok_zero));
	} else {
		VERIFY_ARRAY_VALUE(dst, ok_nonzero, sizeof_u32(ok_nonzero));
	}

fail:
	return ret;
}

/* T23X key wrap and unwrap
 */
#define AES_GCM_IV_SIZE 12U

#define KEYSLOT_SRC_WRAP   6U	// Wrap this keyslot out
#define KEYSLOT_DST_UNWRAP 7U	// Unwrap key to this keyslot

/* KW wrapped key blob (wrapped with t23x_wrapping_key) */
static const uint8_t t23x_kw_blob[64] = {
	0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x89, 0x78, 0xC5, 0xB5, 0x81, 0xF2, 0x87, 0x06, 0xA2, 0x19, 0xC3, 0x83, 0x51, 0xF7, 0xAE, 0xE8,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xBC, 0xF2, 0x71, 0xCC, 0xAC, 0x00, 0x63, 0x07, 0x3B, 0xEA, 0xC5, 0x3A, 0xEB, 0x61, 0x78, 0x79
};

static const uint8_t t23x_wrapping_key[16U] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
						0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };

static status_t TEST_keyop_kuw(crypto_context_t *c, te_crypto_algo_t not_used, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	// 12 byte (96 bit IV supported)
	uint8_t iv[12U] =
		{ 0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce, 0xdb, 0xad,
		  0xde, 0xca, 0xf8, 0x88, };

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_USE_KEYSLOT_KEY | KEY_FLAG_LEAVE_KEY_IN_KEYSLOT,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = 16U,
	};

	(void)not_used;

	/* Clear the key from DST_UNWRAP keyslot (just for the sake of it)
	 */
	ret = se_set_device_kac_keyslot(SE_CDEV_SE0, KEYSLOT_DST_UNWRAP,
					NULL,
					(sizeof_u32(t23x_wrapping_key) * 8U),
					SE_KAC_KEYSLOT_USER,
					SE_KAC_FLAGS_NONE,
					0U,
					TE_ALG_AES_ECB_NOPAD);
	TRAP_IF_ERROR(ret, "Failed to clear key from key slot %u\n", KEYSLOT_DST_UNWRAP);

	ret = keyop_kwuw_key_verification(c, KEYSLOT_DST_UNWRAP, true);
	TRAP_IF_ERROR(ret, "Zero key init test failed");

	/* Then write the UNWRAPPING KEY to AES_KEYSLOT (slot set in akey)
	 * (unexportable PSC domain key, only usable for KEY WRAP and UNWRAP
	 *  operations).
	 */
	ret = se_set_device_kac_keyslot(SE_CDEV_SE0, AES_KEYSLOT,
					t23x_wrapping_key,
					(sizeof_u32(t23x_wrapping_key) * 8U),
					SE_KAC_KEYSLOT_USER,
					SE_KAC_FLAGS_NONE,
					0U,
					TE_ALG_KEYOP_KUW);
	/* The value of this key can not be verified by AES encryption (as such),
	 * but it could be validated by verifying the "KW blob" (deciphering the blob
	 * with AES-GCM) => not done in this test.
	 */
	TRAP_IF_ERROR(ret, "Failed to set wrapping key to key slot %u\n", AES_KEYSLOT);

	LOG_ALWAYS("==> KEY UNWRAP test, unwrapping keyslot with preset key (%u) to slot %u\n",
		   AES_KEYSLOT,
		   KEYSLOT_DST_UNWRAP);

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_DECRYPT, TE_ALG_KEYOP_KUW);

	/* Wrap out a key from the keyslot */
	arg.ca_alg_mode = TE_ALG_MODE_DECRYPT;
	arg.ca_algo     = TE_ALG_KEYOP_KUW;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg.ca_init.engine_hint = eid;

	se_util_mem_move(arg.ca_init.kwuw.iv, iv, AES_GCM_IV_SIZE);

	arg.ca_init.kwuw.wrap_index = KEYSLOT_DST_UNWRAP;

	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src = NULL;		/* No AAD data in this test */
	arg.ca_data.src_size = 0U;
	arg.ca_data.src_kwrap = t23x_kw_blob;
	arg.ca_data.src_kwrap_size = sizeof_u32(t23x_kw_blob);

	ret = CRYPTO_OPERATION(c, &arg);
	TRAP_IF_ERROR(ret, "Key unwrap failed");

	ret = keyop_kwuw_key_verification(c, KEYSLOT_DST_UNWRAP, false);
	TRAP_IF_ERROR(ret, "Unwrapped key test failed");

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

static status_t TEST_keyop_kw_kuw_aad(crypto_context_t *c,
					  te_crypto_algo_t not_used,
					  engine_id_t eid,
					  const uint8_t *aad,
					  uint32_t aad_len_arg)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };
	te_crypto_algo_t algo = TE_ALG_KEYOP_KW;
	uint32_t aad_len = aad_len_arg;

	// 12 byte (96 bit IV supported)
	uint8_t iv[12U] =
		{ 0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce, 0xdb, 0xad,
		  0xde, 0xca, 0xf8, 0x88, };

	// 10U added just to make it larger than necessary (testing purposes)
	uint8_t dst[64U + 10U];
	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = 16U,
		.k_aes_key = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			       0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f },
	};

	(void)not_used;

	/* First set a SE engine key for AES-CBC mode operation (randomly chosen AES mode)
	 * so that the key can be exported (wrapped) out by encrypting it with a key wrap key
	 * in AES-GCM mode wrapping operation.
	 *
	 * The key value is the same as the wrapping key value (key material from akey object).
	 */
	ret = se_set_device_kac_keyslot(SE_CDEV_SE0, KEYSLOT_SRC_WRAP,
					akey.k_aes_key,
					(akey.k_byte_size * 8U),
					SE_KAC_KEYSLOT_USER,
					SE_KAC_FLAGS_EXPORTABLE,
					0U,
					TE_ALG_AES_ECB_NOPAD);
	TRAP_IF_ERROR(ret, "Failed to set an exportable key to key slot %u\n", KEYSLOT_SRC_WRAP);

	ret = keyop_kwuw_key_verification(c, KEYSLOT_SRC_WRAP, false);
	TRAP_IF_ERROR(ret, "SRC key test failed");

	/* Then clear the key from DST_UNWRAP keyslot (just for the sake of it)
	 */
	ret = se_set_device_kac_keyslot(SE_CDEV_SE0, KEYSLOT_DST_UNWRAP,
					NULL,
					(akey.k_byte_size * 8U),
					SE_KAC_KEYSLOT_USER,
					SE_KAC_FLAGS_NONE,
					0U,
					TE_ALG_AES_ECB_NOPAD);
	TRAP_IF_ERROR(ret, "Failed to clear key from key slot %u\n", KEYSLOT_DST_UNWRAP);

	ret = keyop_kwuw_key_verification(c, KEYSLOT_DST_UNWRAP, true);
	TRAP_IF_ERROR(ret, "Zero key init test failed");

	if (65U == aad_len_arg) {
		/* Testing TAG AAD data mismatch error handling in KUW below */
		aad_len = 64U;
	}

	LOG_ALWAYS("==> KEY WRAP test, wrapping keyslot %u, AAD length %u\n",
		   KEYSLOT_SRC_WRAP, aad_len);

	/* Wrap a key from KEYSLOT_SRC_WRAP to a memory blob
	 */
	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, algo);

	/* Wrap out a key from the keyslot */
	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = algo;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for KW\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	// XXX Need to have the IV length field?
	// XXX Not really necessary, KW/KUW use fixed iv lengths as specified in IAS.
	se_util_mem_move(arg.ca_init.kwuw.iv, iv, AES_GCM_IV_SIZE);

	/* Wrap out SRC WRAP index key */
	arg.ca_init.kwuw.wrap_index = KEYSLOT_SRC_WRAP;

	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src = aad;
	arg.ca_data.src_size = aad_len;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	TRAP_IF_ERROR(ret, "Key wrap failed");

	// Make sure the call returns the right amount of bytes!
	TRAP_ASSERT(arg.ca_data.dst_size == 64U);

	DUMP_HEX("KW result:", dst, arg.ca_data.dst_size);

	if (65U == aad_len_arg) {
		/* Testing TAG mismatch error handling in KUW below
		 * KW was created with 64 byte AAD, verify with 63 byte AAD
		 * in the KUW
		 */
		aad_len = 63U;
	}

	LOG_ALWAYS("==> KEY UNWRAP test, unwrapping to keyslot %u, AAD len %u\n",
		   KEYSLOT_DST_UNWRAP, aad_len);

	/* Unwrap a key from wrapped memory blob to KEYSLOT_DST_UNWRAP
	 */
	algo = TE_ALG_KEYOP_KUW;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_DECRYPT, algo);

	/* Wrap out a key from the keyslot */
	arg.ca_alg_mode = TE_ALG_MODE_DECRYPT;
	arg.ca_algo     = algo;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg.ca_init.engine_hint = eid;

	se_util_mem_move(arg.ca_init.kwuw.iv, iv, AES_GCM_IV_SIZE);

	arg.ca_init.kwuw.wrap_index = KEYSLOT_DST_UNWRAP;

	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src = aad;
	arg.ca_data.src_size = aad_len;
	arg.ca_data.src_kwrap = dst;
	arg.ca_data.src_kwrap_size = 64U;

	ret = CRYPTO_OPERATION(c, &arg);
	TRAP_IF_ERROR(ret, "Key unwrap failed");

	ret = keyop_kwuw_key_verification(c, KEYSLOT_DST_UNWRAP, false);
	TRAP_IF_ERROR(ret, "Unwrapped key test failed");

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

static status_t TEST_keyop_kwuw_preset_kwuw_key(crypto_context_t *c, te_crypto_algo_t not_used, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };
	te_crypto_algo_t algo = TE_ALG_KEYOP_KW;

	// 12 byte (96 bit IV supported)
	uint8_t iv[12U] =
		{ 0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce, 0xdb, 0xad,
		  0xde, 0xca, 0xf8, 0x88, };

	// 10U added just to make it larger than necessary (testing purposes)
	uint8_t dst[64U + 10U];
	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_USE_KEYSLOT_KEY | KEY_FLAG_LEAVE_KEY_IN_KEYSLOT,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = 16U,
	};

	(void)not_used;

	/* First set a SE engine key for AES-CBC mode operation (randomly chosen AES mode)
	 * so that the key can be exported (wrapped) out by encrypting it with a key wrap key
	 * in AES-GCM mode wrapping operation.
	 *
	 * The key value is the same as the wrapping key value (key material from akey object).
	 */
	ret = se_set_device_kac_keyslot(SE_CDEV_SE0, KEYSLOT_SRC_WRAP,
					t23x_wrapping_key,
					(sizeof_u32(t23x_wrapping_key) * 8U),
					SE_KAC_KEYSLOT_USER,
					SE_KAC_FLAGS_EXPORTABLE,
					0U,
					TE_ALG_AES_ECB_NOPAD);
	TRAP_IF_ERROR(ret, "Failed to set an exportable key to key slot %u\n", KEYSLOT_SRC_WRAP);

	ret = keyop_kwuw_key_verification(c, KEYSLOT_SRC_WRAP, false);
	TRAP_IF_ERROR(ret, "SRC key test failed");

	/* Then clear the key from DST_UNWRAP keyslot (just for the sake of it)
	 */
	ret = se_set_device_kac_keyslot(SE_CDEV_SE0, KEYSLOT_DST_UNWRAP,
					NULL,
					(sizeof_u32(t23x_wrapping_key) * 8U),
					SE_KAC_KEYSLOT_USER,
					SE_KAC_FLAGS_NONE,
					0U,
					TE_ALG_AES_ECB_NOPAD);
	TRAP_IF_ERROR(ret, "Failed to clear key from key slot %u\n", KEYSLOT_DST_UNWRAP);

	ret = keyop_kwuw_key_verification(c, KEYSLOT_DST_UNWRAP, true);
	TRAP_IF_ERROR(ret, "Zero key init test failed");

	/* These preset the WRAPPING KEY to AES_KEYSLOT (slot used in akey object)
	 * This key has the same value as the key we are going to wrap out, so write
	 * t23x_wrapping_key to AES_KEYSLOT.
	 *
	 * This is an unexportable PSC domain key, only usable for KEY WRAP and UNWRAP
	 * operations.
	 */
	ret = se_set_device_kac_keyslot(SE_CDEV_SE0, AES_KEYSLOT,
					t23x_wrapping_key,
					(sizeof_u32(t23x_wrapping_key) * 8U),
					SE_KAC_KEYSLOT_USER,
					SE_KAC_FLAGS_NONE,
					0U,
					TE_ALG_KEYOP_KWUW);
	/* The value of this key can not be verified by AES encryption (as such),
	 * but it could be validated by verifying the "KW blob" (deciphering the blob
	 * with AES-GCM) => not done in this test.
	 */
	TRAP_IF_ERROR(ret, "Failed to set wrapping key to key slot %u\n", AES_KEYSLOT);

	/* Wrap a key from KEYSLOT_SRC_WRAP to a memory blob
	 */
	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, algo);

	/* Wrap out a key from the keyslot */
	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = algo;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for KW\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	// XXX Need to have the IV length field?
	// XXX Not really necessary, KW/KUW use fixed iv lengths as specified in IAS.
	se_util_mem_move(arg.ca_init.kwuw.iv, iv, AES_GCM_IV_SIZE);

	/* Wrap out SRC WRAP index key */
	arg.ca_init.kwuw.wrap_index = KEYSLOT_SRC_WRAP;

	arg.ca_set_key.kdata = &akey;

	// AAD in <src,src_size>
	arg.ca_data.src = NULL;
	arg.ca_data.src_size = 0U;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	TRAP_IF_ERROR(ret, "Key wrap failed");

	// Make sure the call returns the right amount of bytes!
	TRAP_ASSERT(arg.ca_data.dst_size == 64U);

	DUMP_HEX("KW result:", dst, arg.ca_data.dst_size);

	LOG_ALWAYS("==> KEY UNWRAP test, unwrapping to keyslot %u\n", KEYSLOT_DST_UNWRAP);

	/* Unwrap a key from wrapped memory blob to KEYSLOT_DST_UNWRAP
	 *
	 * This is re-using the same key from AES_KEYSLOT.
	 */
	algo = TE_ALG_KEYOP_KUW;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_DECRYPT, algo);

	/* Wrap out a key from the keyslot */
	arg.ca_alg_mode = TE_ALG_MODE_DECRYPT;
	arg.ca_algo     = algo;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg.ca_init.engine_hint = eid;

	se_util_mem_move(arg.ca_init.kwuw.iv, iv, AES_GCM_IV_SIZE);

	arg.ca_init.kwuw.wrap_index = KEYSLOT_DST_UNWRAP;

	arg.ca_set_key.kdata = &akey;

	// AAD in <src,src_size>
	arg.ca_data.src = NULL;
	arg.ca_data.src_size = 0U;
	arg.ca_data.src_kwrap = dst;
	arg.ca_data.src_kwrap_size = 64U;

	ret = CRYPTO_OPERATION(c, &arg);
	TRAP_IF_ERROR(ret, "Key unwrap failed");

	ret = keyop_kwuw_key_verification(c, KEYSLOT_DST_UNWRAP, false);
	TRAP_IF_ERROR(ret, "Unwrapped key test failed");

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

static status_t run_psc_kw_kuw_tests(crypto_context_t *crypto_ctx, engine_id_t engine_id)
{
	status_t ret = NO_ERROR;
	uint32_t inx = 1U;

	(void)engine_id;

	//TEST_ERRCHK(TEST_keyop_kw_kuw_aad, CCC_ENGINE_ANY, TE_ALG_KEYOP_KW, NULL, 0U);

	// Passing these tests lwrrently requires CCC to set the FINAL bit
	// in crypto_config => It may be this is not intention but HW team
	// is lwrrently verifying this case.
	//
	// Now the FINAL bit is passed from engine_aes_keyop_process_locked()
	// => TODO : check code after Shawn gets back with the result.
	//
	static uint8_t aad[64U] = {
		0xCA, 0xFE, 0xBA, 0xBE, 0xFE, 0xED, 0xDE, 0xCA,
		0xCA, 0xFE, 0xBA, 0xBE, 0xFE, 0xED, 0xDE, 0xCA,
		0xCA, 0xFE, 0xBA, 0xBE, 0xFE, 0xED, 0xDE, 0xCA,
		0xCA, 0xFE, 0xBA, 0xBE, 0xFE, 0xED, 0xDE, 0xCA,
		0xCA, 0xFE, 0xBA, 0xBE, 0xFE, 0xED, 0xDE, 0xCA,
		0xCA, 0xFE, 0xBA, 0xBE, 0xFE, 0xED, 0xDE, 0xCA,
		0xCA, 0xFE, 0xBA, 0xBE, 0xFE, 0xED, 0xDE, 0xCA,
		0xCA, 0xFE, 0xBA, 0xBE, 0xFE, 0xED, 0xDE, 0xCA,
	};

	for (; inx <= sizeof_u32(aad); inx++) {
		TEST_ERRCHK(TEST_keyop_kw_kuw_aad,
			    CCC_ENGINE_ANY, TE_ALG_KEYOP_KW,
			    aad, inx);
	}

#if 0	// XXXX VDK/CMOD does not support KUW results yet...
	/* Negative test which KW's with one AAD and attempts
	 * to KUW it back with one byte shorter AAD.
	 */
	TEST_EXPECT_RET(TEST_keyop_kw_kuw_aad, ERR_NOT_VALID,
			CCC_ENGINE_ANY, TE_ALG_KEYOP_KW, aad, 65U);
#endif

	/* Test T23X key wrapping / key unwrapping; checks key validity.
	 */
	TEST_ERRCHK(TEST_keyop_kw_kuw_aad, CCC_ENGINE_ANY, TE_ALG_KEYOP_KW, NULL, 0U);

	{
		/* Pass AAD lengths 0..63 */
		uint8_t aad_2[64U];
		for (; inx < sizeof_u32(aad_2); inx++) {
			aad_2[inx] = (uint8_t)inx;
			TEST_ERRCHK(TEST_keyop_kw_kuw_aad,
				    CCC_ENGINE_ANY, TE_ALG_KEYOP_KW,
				    aad_2, inx);
		}

		/* Special test case: 64 tests passing 63 byte AAD for KW and
		 * 62 byte AAD for KUW => this must end up in error because KUW
		 * must detect the KW blob tag mismatch.
		 */
		TEST_ERRCHK(TEST_keyop_kw_kuw_aad,
			    CCC_ENGINE_ANY, TE_ALG_KEYOP_KW,
			    aad_2, 64U);
	}

	TEST_ERRCHK(TEST_keyop_kuw,  CCC_ENGINE_ANY, TE_ALG_KEYOP_KUW);
	TEST_ERRCHK(TEST_keyop_kwuw_preset_kwuw_key, CCC_ENGINE_ANY, TE_ALG_KEYOP_KWUW);

	LOG_INFO("[PSC KW/KUW tests: PASSED]\n");
fail:
	return ret;
}
#endif /* HAVE_SE_KAC_KEYOP */

#if HAVE_SE_KAC_KDF

#define KDF_KDK_KEYSLOT	14U	/* KDK key keyslot for KDF 1KEY and 2KEY derivation */
#define KDF_KDD_KEYSLOT	13U	/* KDD key keyslot for KDF 2KEY derivation */

#define KEYSLOT_KDF_DST 7U	/* KDF target keyslot */

#define KEYSLOT_KDF_PURPOSE  CCC_MANIFEST_PURPOSE_CMAC
#define KEYSLOT_KDF_USER     CCC_MANIFEST_USER_PSC
#define KEYSLOT_KDF_KSIZE    128U
#define KEYSLOT_KDF_SW	     0U

static status_t TEST_kdf_1key_derivation(crypto_context_t *c, te_crypto_algo_t algo, engine_id_t eid, uint32_t keysize_bits)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	const char data[] = "This is Derivation Data";

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_KDF,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = KDF_KDK_KEYSLOT,
		// .k_byte_size set below, key material set here
		.k_kdf.kdk = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
			       0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
		},
	};

	switch (keysize_bits) {
	case 128U:
		akey.k_byte_size = 16U;
		break;
	case 192U:
		akey.k_byte_size = 24U;
		break;
	case 256U:
		akey.k_byte_size = 32U;
		break;
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_DERIVE, algo);

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_DERIVE;
	arg.ca_algo     = algo;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for KDF\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	arg.ca_init.kdf.kdf_index    = KEYSLOT_KDF_DST;
	arg.ca_init.kdf.kdf_purpose  = KEYSLOT_KDF_PURPOSE;
	arg.ca_init.kdf.kdf_key_bits = KEYSLOT_KDF_KSIZE;
	arg.ca_init.kdf.kdf_user     = KEYSLOT_KDF_USER;
	arg.ca_init.kdf.kdf_sw       = KEYSLOT_KDF_SW;

	LOG_INFO("KDF target keyslot %u\n", arg.ca_init.kdf.kdf_index);

	// Pass the key params
	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = (const uint8_t *)data;
	arg.ca_data.dst_size = 0U;
	arg.ca_data.dst = NULL;

	LOG_INFO("KDF[0x%x] DERIVATION (COMBINED OPERATION) src %p data\n",
		 algo, data);

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	LOG_INFO("KDF[0x%x] DERIVATION (COMBINED OPERATION) ret %d, attached handle: %u\n",
		 algo, ret, arg.ca_handle);
	CCC_ERROR_CHECK(ret);

	LOG_INFO("KDF DERIVATION: OK\n");
fail:
	CRYPTO_CONTEXT_RESET(c);

	if (NO_ERROR != ret) {
		LOG_INFO("KDF DERIVATION: failed\n");
	}
	return ret;
}

static status_t TEST_kdf_1key_derivation_pre_set_key(crypto_context_t *c, te_crypto_algo_t algo,
						     engine_id_t eid, uint32_t keysize_bits)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	const char data[] = "This is Derivation Data";

	const uint32_t kdf_key32[] = { 0x03020100U, 0x07060504U,
				       0x0b0a0908U, 0x0f0e0d0lw,
				       // for 192 bit key
				       0x13121110U, 0x17161514U,
				       // for 256 bit key
				       0x1b1a1918U, 0x1f1e1d1lw,
	};

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_KDF,
		.k_flags = KEY_FLAG_USE_KEYSLOT_KEY | KEY_FLAG_LEAVE_KEY_IN_KEYSLOT,
		.k_keyslot = KDF_KDK_KEYSLOT,
	};

	switch (keysize_bits) {
	case 128U:
	case 192U:
	case 256U:
		break;
	default:
		// HW only supports a few key sizes
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	TRAP_IF_ERROR(ret, "Invalid KDF key size\n");

	/* Write the KDF KDK key to KDF_KDK_KEYSLOT (slot set in akey)
	 * (unexportable PSC domain key, only usable for KDF KDK
	 *  operations).
	 */
	ret = se_set_device_kac_keyslot(SE_CDEV_SE0, akey.k_keyslot,
					(const uint8_t *)&kdf_key32[0],
					keysize_bits,
					SE_KAC_KEYSLOT_USER,
					SE_KAC_FLAGS_NONE,
					0U,
					algo);

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_DERIVE, algo);

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_DERIVE;
	arg.ca_algo     = algo;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for KDF\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	arg.ca_init.kdf.kdf_index    = KEYSLOT_KDF_DST;
	arg.ca_init.kdf.kdf_purpose  = KEYSLOT_KDF_PURPOSE;
	arg.ca_init.kdf.kdf_key_bits = KEYSLOT_KDF_KSIZE;
	arg.ca_init.kdf.kdf_user     = KEYSLOT_KDF_USER;
	arg.ca_init.kdf.kdf_sw       = KEYSLOT_KDF_SW;

	LOG_INFO("KDF target keyslot %u\n", arg.ca_init.kdf.kdf_index);

	// Pass the key params
	akey.k_byte_size = keysize_bits / 8U;
	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = (const uint8_t *)data;
	arg.ca_data.dst_size = 0U;
	arg.ca_data.dst = NULL;

	LOG_INFO("KDF[0x%x] DERIVATION (COMBINED OPERATION) src %p data\n",
		 algo, data);

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	LOG_INFO("KDF[0x%x] DERIVATION (COMBINED OPERATION) ret %d, attached handle: %u\n",
		 algo, ret, arg.ca_handle);
	CCC_ERROR_CHECK(ret);

	LOG_INFO("KDF DERIVATION: OK\n");
fail:
	CRYPTO_CONTEXT_RESET(c);

	if (NO_ERROR != ret) {
		LOG_INFO("KDF DERIVATION: failed\n");
	}
	return ret;
}

static status_t TEST_kdf_2key_derivation(crypto_context_t *c, te_crypto_algo_t algo, engine_id_t eid, uint32_t keysize_bits)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	const char data[] = "This is Derivation Data";

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_KDF,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = KDF_KDK_KEYSLOT,

		/* .k_byte_size set below, key material set here
		 *
		 * NOTE: FIXME for KDD =>
		 * XXX There is actually nothing that requires KDK key and KDD key to be of same size =>
		 * XXX should support that case as well (need KDD key size parameter)
		 *
		 * XXX Also, the k_flags now specify that both KDK and KDD keys are written or both
		 * XXX  are using existing keys from keyslots => not now possible to use one existing
		 * XXX  and one written key
		 *
		 * XXX NOTE: Same for XTS keys => probably should support such settings.
		 */
		.k_kdf.kdk = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
			       0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f },
		.k_kdf.kdd = { 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
			       0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
		},

		/* KDD key for 2-key KDF variant (ignored for 1-KEY variant)
		 *
		 * KDD key material in .k_kdf_keyslot above at element .k_kdf.kdd
		 */
		.k_kdd_keyslot = KDF_KDD_KEYSLOT,
	};

	switch (keysize_bits) {
	case 128U:
		akey.k_byte_size = 16U;
		break;
	case 192U:
		akey.k_byte_size = 24U;
		break;
	case 256U:
		akey.k_byte_size = 32U;
		break;
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_DERIVE, algo);

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_DERIVE;
	arg.ca_algo     = algo;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for KDF\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	arg.ca_init.kdf.kdf_index    = KEYSLOT_KDF_DST;
	arg.ca_init.kdf.kdf_purpose  = KEYSLOT_KDF_PURPOSE;
	arg.ca_init.kdf.kdf_key_bits = KEYSLOT_KDF_KSIZE;
	arg.ca_init.kdf.kdf_user     = KEYSLOT_KDF_USER;
	arg.ca_init.kdf.kdf_sw       = KEYSLOT_KDF_SW;

	LOG_INFO("KDF 2KEY target keyslot %u\n", arg.ca_init.kdf.kdf_index);

	// Pass the key params
	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = (const uint8_t *)data;
	arg.ca_data.dst_size = 0U;
	arg.ca_data.dst = NULL;

	LOG_INFO("KDF[0x%x] DERIVATION (COMBINED OPERATION) src %p data\n",
		 algo, data);

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	LOG_INFO("KDF[0x%x] DERIVATION (COMBINED OPERATION) ret %d, attached handle: %u\n",
		 algo, ret, arg.ca_handle);
	CCC_ERROR_CHECK(ret);

	LOG_INFO("KDF DERIVATION: OK\n");
fail:
	CRYPTO_CONTEXT_RESET(c);

	if (NO_ERROR != ret) {
		LOG_INFO("KDF DERIVATION: failed\n");
	}
	return ret;
}

static status_t TEST_kdf_2key_derivation_pre_set_key(crypto_context_t *c, te_crypto_algo_t algo,
						     engine_id_t eid, uint32_t keysize_bits)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	const char data[] = "This is Derivation Data";

	/* .k_byte_size set below, key material set here 2KEY KDF key2
	 * catenated at k_byte_size offset from beginning of array =>
	 * this is long enough for 2*256 bit keys
	 *
	 * NOTE: FIXME for KDD =>
	 * XXX There is actually nothing that requires KDK key and KDD
	 * key to be of same size => XXX should support that case as
	 * well (need KDD key size parameter)
	 *
	 * XXX Also, the k_flags now specify that both KDK and KDD keys are written or both
	 * XXX  are using existing keys from keyslots => not now possible to use one existing
	 * XXX  and one written key
	 *
	 * XXX NOTE: Same for XTS keys => probably should support such settings.
	 */
	const uint8_t kdf_key[] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
	};

	const uint8_t kdd_key[] = {
		0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	};

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_KDF,
		.k_flags = KEY_FLAG_USE_KEYSLOT_KEY | KEY_FLAG_LEAVE_KEY_IN_KEYSLOT,
		.k_keyslot = KDF_KDK_KEYSLOT,

		/* KDD keyslot for 2-key KDF variant (ignored for 1-KEY variant)
		 */
		.k_kdd_keyslot = KDF_KDD_KEYSLOT,
	};

	switch (keysize_bits) {
	case 128U:
	case 192U:
	case 256U:
		break;
	default:
		// HW only supports a few key sizes
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	TRAP_IF_ERROR(ret, "Invalid KDF key size\n");

	/* Write the KDF KDK key to KDF_KDK_KEYSLOT (slot set in akey)
	 * (unexportable PSC domain key, only usable for KDF
	 *  operations).
	 *
	 * NOTE: Algorithm is TE_ALG_KEYOP_KDF_2KEY but we need to set the
	 * KDK key. This can be done in two ways:
	 *
	 * 1) Pass TE_ALG_KEYOP_KDF_1KEY as the algorithm, it gets map to KDK key purpose
	 *    (TE_ALG_KEYOP_KDF_1KEY purpose is always KDK key)
	 *
	 *    OR
	 *
	 * 2) Set the kac_flag SE_KAC_FLAGS_PURPOSE_SECONDARY_KEY.
	 * This informs the manifest purpose
	 *    mapper that this is the "other key". When this is given,
	 *    the TE_ALG_KEYOP_KDF_2KEY also gets mapped to KDK purpose; otherwise it is
	 *    mapped to the KDD purpose)
	 */
	ret = se_set_device_kac_keyslot(SE_CDEV_SE0, akey.k_keyslot,
					&kdf_key[0],
					keysize_bits,
					SE_KAC_KEYSLOT_USER,
					SE_KAC_FLAGS_PURPOSE_SECONDARY_KEY,
					0U,
					algo);
	TRAP_IF_ERROR(ret, "KDK key setting failed\n");

	/* Write the KSF KDD key to KDF_KDD_KEYSLOT (slot set in akey)
	 * (unexportable PSC domain key, only usable for KDF
	 *  operations).
	 */
	ret = se_set_device_kac_keyslot(SE_CDEV_SE0, akey.k_kdd_keyslot,
					&kdd_key[0],
					keysize_bits,
					SE_KAC_KEYSLOT_USER,
					SE_KAC_FLAGS_NONE,
					0U,
					algo);
	TRAP_IF_ERROR(ret, "KDD key setting failed\n");

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_DERIVE, algo);

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_DERIVE;
	arg.ca_algo     = algo;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for KDF\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	arg.ca_init.kdf.kdf_index    = KEYSLOT_KDF_DST;
	arg.ca_init.kdf.kdf_purpose  = KEYSLOT_KDF_PURPOSE;
	arg.ca_init.kdf.kdf_key_bits = KEYSLOT_KDF_KSIZE;
	arg.ca_init.kdf.kdf_user     = KEYSLOT_KDF_USER;
	arg.ca_init.kdf.kdf_sw       = KEYSLOT_KDF_SW;

	LOG_INFO("KDF 2KEY target keyslot %u\n", arg.ca_init.kdf.kdf_index);

	// Pass the key params
	akey.k_byte_size = keysize_bits / 8U;
	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = (const uint8_t *)data;
	arg.ca_data.dst_size = 0U;
	arg.ca_data.dst = NULL;

	LOG_INFO("KDF[0x%x] DERIVATION (COMBINED OPERATION) src %p data\n",
		 algo, data);

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	LOG_INFO("KDF[0x%x] DERIVATION (COMBINED OPERATION) ret %d, attached handle: %u\n",
		 algo, ret, arg.ca_handle);
	CCC_ERROR_CHECK(ret);

	LOG_INFO("KDF 2-KEY DERIVATION: OK\n");
fail:
	CRYPTO_CONTEXT_RESET(c);

	if (NO_ERROR != ret) {
		LOG_INFO("KDF 2-KEY DERIVATION: failed\n");
	}
	return ret;
}
#endif // HAVE_SE_KAC_KDF

#if HAVE_HW_HMAC_SHA
static status_t TEST_hw_hmac_empty(crypto_context_t *c, te_crypto_algo_t algo, engine_id_t eid, uint32_t key_bitsize)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	static uint8_t mac[64];	// largest mac value
	uint32_t mac_size = 0U;
	static uint8_t key[32];
	static te_args_key_data_t akey;

	switch (key_bitsize) {
	case 128U:
	case 192U:
	case 256U:
		break;
	default:
		// HW HMAC only supports a few key sizes
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, algo);

	se_util_mem_set((uint8_t *)&akey, 0U, sizeof_u32(akey));
	se_util_mem_set(mac, 0U, sizeof_u32(mac));
	se_util_mem_set(key, 0U, sizeof_u32(key));

	/* Preset fields, do everythin in one syscall */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = algo;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	// Set the MAC key
	//
	akey.k_key_type = KEY_TYPE_HMAC;

	/* TODO: Different kind of tests, add a function parameter to select.
	 */
#if 1
	akey.k_flags = KEY_FLAG_FORCE_KEYSLOT;
#else
	akey.k_flags = KEY_FLAG_FORCE_KEYSLOT | KEY_FLAG_LEAVE_KEY_IN_KEYSLOT;
#endif
	akey.k_byte_size = (key_bitsize / 8U);
	akey.k_keyslot = HMAC_KEYSLOT;

	se_util_mem_move(akey.k_hmac_key, key, akey.k_byte_size);

	LOG_INFO("Hint: use engine 0x%x (%s) for HMAC digest\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	// Pass the key params
	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = 0;
	arg.ca_data.src = NULL;
	arg.ca_data.dst_size = sizeof_u32(mac);
	arg.ca_data.dst = mac;

	LOG_INFO("MAC(COMBINED OPERATION) NULL data, HASH ADDR %p\n", mac);

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	LOG_INFO("MAC(COMBINED => RETURN) handle %u: ret %d\n", arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	mac_size = arg.ca_data.dst_size;

	/* verify that the result is correct */

	const uint8_t hm_sha224[28] = {
		0x5c, 0xe1, 0x4f, 0x72, 0x89, 0x46, 0x62, 0x21,
		0x3e, 0x27, 0x48, 0xd2, 0xa6, 0xba, 0x23, 0x4b,
		0x74, 0x26, 0x39, 0x10, 0xce, 0xdd, 0xe2, 0xf5,
		0xa9, 0x27, 0x15, 0x24
	};

	const uint8_t hm_sha256[32] = {
		0xb6, 0x13, 0x67, 0x9a, 0x08, 0x14, 0xd9, 0xec,
		0x77, 0x2f, 0x95, 0xd7, 0x78, 0xc3, 0x5f, 0xc5,
		0xff, 0x16, 0x97, 0xc4, 0x93, 0x71, 0x56, 0x53,
		0xc6, 0xc7, 0x12, 0x14, 0x42, 0x92, 0xc5, 0xad
	};

	const uint8_t hm_sha384[48] = {
		0x6c, 0x1f, 0x2e, 0xe9, 0x38, 0xfa, 0xd2, 0xe2,
		0x4b, 0xd9, 0x12, 0x98, 0x47, 0x43, 0x82, 0xca,
		0x21, 0x8c, 0x75, 0xdb, 0x3d, 0x83, 0xe1, 0x14,
		0xb3, 0xd4, 0x36, 0x77, 0x76, 0xd1, 0x4d, 0x35,
		0x51, 0x28, 0x9e, 0x75, 0xe8, 0x20, 0x9c, 0xd4,
		0xb7, 0x92, 0x30, 0x28, 0x40, 0x23, 0x4a, 0xdc
	};

	const uint8_t hm_sha512[64] = {
		0xb9, 0x36, 0xce, 0xe8, 0x6c, 0x9f, 0x87, 0xaa,
		0x5d, 0x3c, 0x6f, 0x2e, 0x84, 0xcb, 0x5a, 0x42,
		0x39, 0xa5, 0xfe, 0x50, 0x48, 0x0a, 0x6e, 0xc6,
		0x6b, 0x70, 0xab, 0x5b, 0x1f, 0x4a, 0xc6, 0x73,
		0x0c, 0x6c, 0x51, 0x54, 0x21, 0xb3, 0x27, 0xec,
		0x1d, 0x69, 0x40, 0x2e, 0x53, 0xdf, 0xb4, 0x9a,
		0xd7, 0x38, 0x1e, 0xb0, 0x67, 0xb3, 0x38, 0xfd,
		0x7b, 0x0c, 0xb2, 0x22, 0x47, 0x22, 0x5d, 0x47
	};

	const uint8_t *correct = NULL;

	switch (algo) {
	case TE_ALG_HMAC_SHA224:
		correct = hm_sha224;
		TRAP_ASSERT(mac_size == 28U);
		break;
	case TE_ALG_HMAC_SHA256:
		correct = hm_sha256;
		TRAP_ASSERT(mac_size == 32U);
		break;
	case TE_ALG_HMAC_SHA384:
		correct = hm_sha384;
		TRAP_ASSERT(mac_size == 48U);
		break;
	case TE_ALG_HMAC_SHA512:
		correct = hm_sha512;
		TRAP_ASSERT(mac_size == 64U);
		break;
	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		LOG_INFO("Correct result not defined for MAC 0x%x\n", algo);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (128U != key_bitsize) {
		LOG_ERROR("HW HMAC results verified only with 128 bit keys\n");
		correct = NULL;
	}

	if (NULL != correct) {
		VERIFY_ARRAY_VALUE(mac, correct, mac_size);
	} else {
		LOG_ERROR("%s: algo 0x%x result not verified\n", __func__, algo);
	}

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

static status_t TEST_hw_hmac_4bytes(crypto_context_t *c, te_crypto_algo_t algo, engine_id_t eid, uint32_t key_bitsize)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	uint8_t data[4] = { 'h', 'm', 'a', 'c' };

	static uint8_t mac[64];	// largest mac value
	uint32_t mac_size = 0U;
	static uint8_t key[32];
	static te_args_key_data_t akey;

	switch (key_bitsize) {
	case 128U:
	case 192U:
	case 256U:
		break;
	default:
		// HW HMAC only supports a few key sizes
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, algo);

	se_util_mem_set((uint8_t *)&akey, 0U, sizeof_u32(akey));
	se_util_mem_set(mac, 0U, sizeof_u32(mac));
	se_util_mem_set(key, 0U, sizeof_u32(key));

	/* Preset fields, do everythin in one syscall */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = algo;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	// Set the MAC key
	//
	akey.k_key_type = KEY_TYPE_HMAC;
#if 0
	// Use the hybrid SW/HW CCC version of HMAC-SHA
	akey.k_flags = KEY_FLAG_NONE;
#else
	// Use the HMAC-SHA2 HW via SHA keyslot
	akey.k_flags = KEY_FLAG_FORCE_KEYSLOT;
#endif
	akey.k_byte_size = (key_bitsize / 8U);
	akey.k_keyslot = HMAC_KEYSLOT;

	se_util_mem_move(akey.k_hmac_key, key, akey.k_byte_size);

	LOG_INFO("Hint: use engine 0x%x (%s) for HMAC digest\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	// Pass the key params
	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(mac);
	arg.ca_data.dst = mac;

	LOG_INFO("MAC(COMBINED OPERATION) NULL data, HASH ADDR %p\n", mac);

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	LOG_INFO("MAC(COMBINED => RETURN) handle %u: ret %d\n", arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	mac_size = arg.ca_data.dst_size;

	/* verify that the result is correct */

	// XXXXX FIXME!!!
	const uint8_t hm_sha224[28] = {
		0x00
	};

	const uint8_t hm_sha256[32] = {
		0x95, 0x67, 0x7e, 0x8c, 0xe8, 0x59, 0xfa, 0x5b, 0x86, 0x16, 0xfd, 0x52, 0x34, 0x83, 0xe1, 0x97,
		0x13, 0x36, 0xa5, 0x37, 0x01, 0xb6, 0x94, 0x3e, 0x07, 0x65, 0xac, 0x14, 0xbb, 0x78, 0xe1, 0x51,
	};

	const uint8_t hm_sha384[48] = {
		0x00
	};

	const uint8_t hm_sha512[64] = {
		0x00
	};

	const uint8_t *correct = NULL;

	switch (algo) {
	case TE_ALG_HMAC_SHA224:
		correct = hm_sha224;
		TRAP_ASSERT(mac_size == 28U);
		break;
	case TE_ALG_HMAC_SHA256:
		correct = hm_sha256;
		TRAP_ASSERT(mac_size == 32U);
		break;
	case TE_ALG_HMAC_SHA384:
		correct = hm_sha384;
		TRAP_ASSERT(mac_size == 48U);
		break;
	case TE_ALG_HMAC_SHA512:
		correct = hm_sha512;
		TRAP_ASSERT(mac_size == 64U);
		break;
	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		LOG_INFO("Correct result not defined for MAC 0x%x\n", algo);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (128U != key_bitsize) {
		LOG_ERROR("HW HMAC results verified only with 128 bit keys\n");
		correct = NULL;
	}

	if (NULL != correct) {
		VERIFY_ARRAY_VALUE(mac, correct, mac_size);
	} else {
		LOG_ERROR("%s: algo 0x%x result not verified\n", __func__, algo);
	}

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

// Max 100 bytes of data for the hmac tests
static uint8_t hmac_data[ 100 ] = { 0x0U };

static status_t TEST_hw_hmac_0_to_100_bytes(crypto_context_t *c, te_crypto_algo_t algo, engine_id_t eid, uint32_t data_byte_size)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	static uint8_t mac[64];	// largest mac value
	uint32_t mac_size = 0U;
	static uint8_t key[32];
	static te_args_key_data_t akey;
	uint32_t key_bitsize = 128U;

	TRAP_ASSERT(data_byte_size <= sizeof_u32(hmac_data));

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, algo);

	se_util_mem_set((uint8_t *)&akey, 0U, sizeof_u32(akey));
	se_util_mem_set(mac, 0U, sizeof_u32(mac));
	se_util_mem_set(key, 0U, sizeof_u32(key));

	/* Preset fields, do everythin in one syscall */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = algo;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	// Set the MAC key
	//
	akey.k_key_type = KEY_TYPE_HMAC;
#if 0
	// Use the hybrid SW/HW CCC version of HMAC-SHA
	akey.k_flags = KEY_FLAG_NONE;
#else
	// Use the HMAC-SHA2 HW via SHA keyslot
	akey.k_flags = KEY_FLAG_FORCE_KEYSLOT;
#endif
	akey.k_byte_size = (key_bitsize / 8U);
	akey.k_keyslot = HMAC_KEYSLOT;

	se_util_mem_move(akey.k_hmac_key, key, akey.k_byte_size);

	LOG_INFO("Hint: use engine 0x%x (%s) for HMAC digest\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	// Pass the key params
	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = data_byte_size;
	arg.ca_data.src = hmac_data;
	arg.ca_data.dst_size = sizeof_u32(mac);
	arg.ca_data.dst = mac;

	LOG_INFO("MAC(COMBINED OPERATION) NULL data, HASH ADDR %p\n", mac);

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	LOG_INFO("MAC(COMBINED => RETURN) handle %u: ret %d\n", arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	mac_size = arg.ca_data.dst_size;

	switch (algo) {
	case TE_ALG_HMAC_SHA224:
		TRAP_ASSERT(mac_size == 28U);
		break;
	case TE_ALG_HMAC_SHA256:
		TRAP_ASSERT(mac_size == 32U);
		break;
	case TE_ALG_HMAC_SHA384:
		TRAP_ASSERT(mac_size == 48U);
		break;
	case TE_ALG_HMAC_SHA512:
		TRAP_ASSERT(mac_size == 64U);
		break;
	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}
	CCC_ERROR_CHECK(ret);

	LOG_ERROR("%s: algo 0x%x result not verified\n", __func__, algo);
	LOG_HEXBUF("HMAC-SHA result", mac, mac_size);

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

static status_t TEST_hw_hmac_pre_set_key(crypto_context_t *c, te_crypto_algo_t algo, engine_id_t eid,
					 uint32_t key_bitsize, uint32_t data_byte_size)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	uint8_t mac[64] = { 0x0U };	// largest mac value
	uint32_t mac_size = 0U;

	/* Fixed key material for 128, 192 and 256 bit keys in the tests
	 * Bytes in word are here in little endian (reversed), e.g.
	 * the 16 byte key values is 000102030405060708090a0b0c0d0e0f
	 */
	static uint32_t hmac_key32[] = { 0x03020100U, 0x07060504U,
					 0x0b0a0908U, 0x0f0e0d0lw,
					 0x13121110U, 0x17161514U,
					 0x1b1a1918U, 0x1f1e1d1lw,
	};

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_HMAC,
		.k_flags = KEY_FLAG_USE_KEYSLOT_KEY | KEY_FLAG_LEAVE_KEY_IN_KEYSLOT,
		.k_keyslot = HMAC_KEYSLOT,
	};

	TRAP_ASSERT(data_byte_size <= sizeof_u32(hmac_data));

	switch (key_bitsize) {
	case 128U:
	case 192U:
	case 256U:
		break;
	default:
		// HW HMAC only supports a few key sizes
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	TRAP_IF_ERROR(ret, "Invalid HMAC key size for HW HMAC-HA2 operation\n");

	/* Write the HMAC-SHA2 key to SHA_KEYSLOT (slot set in akey)
	 * (unexportable PSC domain key, only usable for HMAC
	 *  operations).
	 */
	ret = se_set_device_kac_keyslot(SE_CDEV_SE0, akey.k_keyslot,
					(const uint8_t *)hmac_key32,
					key_bitsize,
					SE_KAC_KEYSLOT_USER,
					SE_KAC_FLAGS_NONE,
					0U,
					algo);

	TRAP_IF_ERROR(ret, "Failed to set wrapping key to key slot %u\n",
		      akey.k_keyslot);

	LOG_ALWAYS("%u bit HMAC key written to keyslot %u\n",
		   key_bitsize, akey.k_keyslot);

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, algo);

	/* Preset fields, do everythin in one syscall
	 */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = algo;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for HMAC digest\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	/* Value not static, initialize at runtime...
	 */
	akey.k_byte_size = (key_bitsize / 8U);

	// Pass the key params
	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = data_byte_size;
	arg.ca_data.src = hmac_data;
	arg.ca_data.dst_size = sizeof_u32(mac);
	arg.ca_data.dst = mac;

	LOG_INFO("MAC(COMBINED OPERATION) NULL data, HASH ADDR %p\n", mac);

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	LOG_INFO("MAC(COMBINED => RETURN) handle %u: ret %d\n", arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	mac_size = arg.ca_data.dst_size;

	/* verify that the result is correct
	 */
	const uint8_t hm_sha224_k128[28] = {
	};

	// 1 zero byte input
	const uint8_t hm_sha256_k128[32] = {
		0xec, 0x5a, 0xd4, 0x8c, 0x9c, 0x15, 0x22, 0x49, 0x55, 0x60, 0xb7, 0x0a, 0x0a, 0x05, 0x72, 0x9c,
		0xa2, 0x69, 0x35, 0x7c, 0x7e, 0xca, 0xb0, 0xb3, 0xc2, 0x14, 0x00, 0x83, 0x62, 0x2b, 0x90, 0x30,
	};

	const uint8_t hm_sha384_k128[48] = {
	};

	const uint8_t hm_sha512_k128[64] = {
	};

	const uint8_t *correct = NULL;

	switch (algo) {
	case TE_ALG_HMAC_SHA224:
		correct = hm_sha224_k128;
		TRAP_ASSERT(mac_size == 28U);
		break;
	case TE_ALG_HMAC_SHA256:
		if (1U == data_byte_size) {
			correct = hm_sha256_k128;
		}
		TRAP_ASSERT(mac_size == 32U);
		break;
	case TE_ALG_HMAC_SHA384:
		correct = hm_sha384_k128;
		TRAP_ASSERT(mac_size == 48U);
		break;
	case TE_ALG_HMAC_SHA512:
		correct = hm_sha512_k128;
		TRAP_ASSERT(mac_size == 64U);
		break;
	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		LOG_INFO("Correct result not defined for MAC 0x%x\n", algo);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (128U != key_bitsize) {
		LOG_ERROR("HW HMAC results verified only with 128 bit keys\n");
		correct = NULL;
	}

	if (NULL != correct) {
		VERIFY_ARRAY_VALUE(mac, correct, mac_size);
	} else {
		LOG_ALWAYS("%s: algo 0x%x result not verified: %u zero bytes\n",
			   __func__, algo, data_byte_size);
		LOG_HEXBUF("HMAC-SHA result, %u zero bytes", mac, mac_size);
	}

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}
#endif /* HAVE_HW_HMAC_SHA */

status_t run_psc_tests(crypto_context_t *crypto_ctx, engine_id_t engine_id)
{
	status_t ret = NO_ERROR;

	(void)engine_id;

#if HAVE_HW_HMAC_SHA
	uint32_t inx = 0U;

	// VDK CMOD has a bug => longer than 64 byte HMAC-SHA2 HW operations fail....

	for (inx = 0U; inx < 65U; inx++) {
		TEST_ERRCHK(TEST_hw_hmac_0_to_100_bytes, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA224, inx);
	}

	for (inx = 0U; inx < 65U; inx++) {
		TEST_ERRCHK(TEST_hw_hmac_pre_set_key, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA256, 128U, inx);
	}
	TEST_ERRCHK(TEST_hw_hmac_4bytes, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA256, 128U);
	TEST_ERRCHK(TEST_hw_hmac_empty, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA256, 128U);
#endif /* HAVE_HW_HMAC_SHA */

#if PSC_CIPHER_DECIPHER_TESTS
	TEST_ERRCHK(TEST_psc_cipher_decipher, CCC_ENGINE_SE0_AES0, TE_ALG_AES_CBC_NOPAD, 128U);
	TEST_ERRCHK(TEST_psc_decipher_fixed_key, CCC_ENGINE_SE0_AES0, TE_ALG_AES_CBC_NOPAD, 128U);

	TEST_ERRCHK(TEST_psc_cipher_decipher, CCC_ENGINE_SE0_AES0, TE_ALG_AES_CBC_NOPAD, 192U);
	TEST_ERRCHK(TEST_psc_decipher_fixed_key, CCC_ENGINE_SE0_AES0, TE_ALG_AES_CBC_NOPAD, 192U);

	TEST_ERRCHK(TEST_psc_cipher_decipher, CCC_ENGINE_SE0_AES0, TE_ALG_AES_CBC_NOPAD, 256U);
	TEST_ERRCHK(TEST_psc_decipher_fixed_key, CCC_ENGINE_SE0_AES0, TE_ALG_AES_CBC_NOPAD, 256U);

	TEST_ERRCHK(TEST_psc_cipher_decipher, CCC_ENGINE_SE0_AES1, TE_ALG_AES_CBC_NOPAD, 128U);
	TEST_ERRCHK(TEST_psc_decipher_fixed_key, CCC_ENGINE_SE0_AES1, TE_ALG_AES_CBC_NOPAD, 128U);

	TEST_ERRCHK(TEST_psc_cipher_decipher, CCC_ENGINE_SE0_AES1, TE_ALG_AES_CBC_NOPAD, 192U);
	TEST_ERRCHK(TEST_psc_decipher_fixed_key, CCC_ENGINE_SE0_AES1, TE_ALG_AES_CBC_NOPAD, 192U);

	TEST_ERRCHK(TEST_psc_cipher_decipher, CCC_ENGINE_SE0_AES1, TE_ALG_AES_CBC_NOPAD, 256U);
	TEST_ERRCHK(TEST_psc_decipher_fixed_key, CCC_ENGINE_SE0_AES1, TE_ALG_AES_CBC_NOPAD, 256U);
#endif /* PSC_CIPHER_DECIPHER_TESTS */

#if HAVE_SE_KAC_KDF
	TEST_ERRCHK(TEST_kdf_1key_derivation_pre_set_key, CCC_ENGINE_ANY, TE_ALG_KEYOP_KDF_1KEY, 128U);

	TEST_ERRCHK(TEST_kdf_2key_derivation_pre_set_key, CCC_ENGINE_ANY, TE_ALG_KEYOP_KDF_2KEY, 128U);
	TEST_ERRCHK(TEST_kdf_2key_derivation_pre_set_key, CCC_ENGINE_ANY, TE_ALG_KEYOP_KDF_2KEY, 192U);
	TEST_ERRCHK(TEST_kdf_2key_derivation_pre_set_key, CCC_ENGINE_ANY, TE_ALG_KEYOP_KDF_2KEY, 256U);

	/* KDF 1-KEY derivation with 128, 192 and 256 bit keys */
	TEST_ERRCHK(TEST_kdf_1key_derivation, CCC_ENGINE_ANY, TE_ALG_KEYOP_KDF_1KEY, 128U);
	TEST_ERRCHK(TEST_kdf_1key_derivation, CCC_ENGINE_ANY, TE_ALG_KEYOP_KDF_1KEY, 192U);
	TEST_ERRCHK(TEST_kdf_1key_derivation, CCC_ENGINE_ANY, TE_ALG_KEYOP_KDF_1KEY, 256U);

	/* KDF 2-KEY derivation with 128, 192 and 256 bit keys */
	TEST_ERRCHK(TEST_kdf_2key_derivation, CCC_ENGINE_ANY, TE_ALG_KEYOP_KDF_2KEY, 128U);
	TEST_ERRCHK(TEST_kdf_2key_derivation, CCC_ENGINE_ANY, TE_ALG_KEYOP_KDF_2KEY, 192U);
	TEST_ERRCHK(TEST_kdf_2key_derivation, CCC_ENGINE_ANY, TE_ALG_KEYOP_KDF_2KEY, 256U);
#endif /* HAVE_SE_KAC_KDF */

#if HAVE_SE_KAC_KEYOP
	TEST_SUITE("PSC-tests (KW/KUW)", run_psc_kw_kuw_tests(crypto_ctx, CCC_ENGINE_SE0_AES0));
#endif /* HAVE_SE_KAC_KEYOP */

	if (CFALSE) {
	fail:
		LOG_ERROR("[ ***ERROR: SE TEST CASE FAILED! (err 0x%x) ]\n",ret);
		// On failure must return error code
		if (NO_ERROR == ret) {
			ret = SE_ERROR(ERR_GENERIC);
		}
	}

	return ret;
}
#endif /* KERNEL_TEST_MODE && CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T23X) */
