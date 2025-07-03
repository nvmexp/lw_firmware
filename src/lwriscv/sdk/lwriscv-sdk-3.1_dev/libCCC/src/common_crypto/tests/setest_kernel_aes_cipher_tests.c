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

#if !defined(AES_KEYSLOT)
#define AES_KEYSLOT 2U
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if HAVE_SE_AES
static status_t TEST_cipher(crypto_context_t *c, te_crypto_algo_t aes_mode, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	unsigned char data[16] = "abcdefghijklmnop";
	DMA_ALIGN_DATA uint8_t dst[16];
	uint32_t written = 0U;
	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = 16,
		.k_aes_key = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	};

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, aes_mode);

	se_util_mem_set(dst, 0U, sizeof_u32(dst));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = aes_mode;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	se_util_mem_set(arg.ca_init.aes.ci_iv, 0U, 16U);

	if (aes_mode == TE_ALG_AES_CTR) {
		arg.ca_init.aes.ctr_mode.ci_increment = 1;
		// Constant CTR mode preset value is a security issue if key is constant
		// but hey, this is just a test...
		se_util_mem_set(arg.ca_init.aes.ctr_mode.ci_counter, 0U, 16U);
	}

	arg.ca_opcode = TE_OP_INIT;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	LOG_INFO("CRYPTO(init): ALGO: 0x%x, ret %d (sizeof arg %u)\n",
		   aes_mode, ret, sizeof_u32(arg));
	CCC_ERROR_CHECK(ret);

	arg.ca_set_key.kdata = &akey;
	arg.ca_opcode = TE_OP_SET_KEY;

	ret = CRYPTO_OPERATION(c, &arg);

	LOG_INFO("CRYPTO(set_key) handle %u: ret %d\n",
		   arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	arg.ca_opcode = TE_OP_DOFINAL;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("CRYPTO(dofinal) handle %u: ret %d\n",
		   arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	written = arg.ca_data.dst_size;
	LOG_INFO("CRYPTO(after dofinal): DST_SIZE %u\n", written);

	DUMP_HEX("CIPHER RESULT:", dst, sizeof_u32(dst));

	CRYPTO_CONTEXT_RESET(c);
	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_DECRYPT, aes_mode);

	/* decipher hte previous result and compare with original */

	LOG_INFO("Decipher data ciphered above and compare that the result matches original data\n");

	se_util_mem_set((uint8_t *)&arg, 0U, sizeof_u32(arg));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_DECRYPT;
	arg.ca_algo     = aes_mode;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	se_util_mem_set(arg.ca_init.aes.ci_iv, 0U, 16U);

	if (aes_mode == TE_ALG_AES_CTR) {
		// Use default: counter 0 maps to 1
		// arg.ca_init.aes.ctr_mode.ci_increment = 1;

		// Constant CTR mode preset value is a security issue if key is constant
		// but hey, this is just a test...
		se_util_mem_set(arg.ca_init.aes.ctr_mode.ci_counter, 0U, 16U);
	}

	arg.ca_set_key.kdata = &akey;

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
	CCC_ERROR_CHECK(ret);

	DUMP_HEX("DECIPHER RESULT:", dst, sizeof_u32(dst));

	VERIFY_ARRAY_VALUE(dst, data, sizeof_u32(data));

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

#ifdef TEST_AES_IV_PRESET
static status_t TEST_cipher_with_preset_key_iv(crypto_context_t *c, te_crypto_algo_t aes_mode, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	unsigned char data[16U] = "abcdefghijklmnop";
	DMA_ALIGN_DATA uint8_t dst[16U] = { 0x0U };
	uint32_t written = 0U;
	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = 16U,
		.k_aes_key = { 0x0fU, 0x0eU, 0x0dU, 0x0lw, 0x0bU, 0x0aU, 0x09U, 0x08U,
			       0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U },
	};
	uint8_t iv[16U] = { 0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U,
				   0x0fU, 0x0eU, 0x0dU, 0x0lw, 0x0bU, 0x0aU, 0x09U, 0x08U };

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, aes_mode);

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = aes_mode;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	ret = se_set_device_aes_keyslot(SE_CDEV_SE0, AES_KEYSLOT, akey.k_aes_key, 128U);
	CCC_ERROR_CHECK(ret);
	/* use the key preset by se_set_device_aes_keyslot() */
	akey.k_flags |= KEY_FLAG_USE_KEYSLOT_KEY;

	if (TE_ALG_AES_CTR == aes_mode) {
		arg.ca_init.aes.ctr_mode.ci_increment = 1U;
		/* Constant CTR mode preset value is a security issue if key is constant
		 * but hey, this is just a test...
		 */
		se_util_mem_set(arg.ca_init.aes.ctr_mode.ci_counter, 0U, 16U);
	} else {
		ret = se_set_device_aes_keyslot_iv(SE_CDEV_SE0, AES_KEYSLOT, iv);
		CCC_ERROR_CHECK(ret);
		/* use the IV preset by se_set_device_aes_keyslot_iv() */
		arg.ca_init.aes.flags |= INIT_FLAG_CIPHER_USE_PRESET_IV;
	}

	arg.ca_opcode = TE_OP_INIT;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	LOG_INFO("CRYPTO(init): ALGO: 0x%x, ret %d (sizeof arg %u)\n",
		   aes_mode, ret, sizeof_u32(arg));
	CCC_ERROR_CHECK(ret);

	arg.ca_set_key.kdata = &akey;
	arg.ca_opcode = TE_OP_SET_KEY;

	ret = CRYPTO_OPERATION(c, &arg);

	LOG_INFO("CRYPTO(set_key) handle %u: ret %d\n",
		   arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	arg.ca_opcode = TE_OP_DOFINAL;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("CRYPTO(dofinal) handle %u: ret %d\n",
		   arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	written = arg.ca_data.dst_size;
	LOG_INFO("CRYPTO(after dofinal): DST_SIZE %u\n", written);

	DUMP_HEX("CIPHER RESULT:", dst, sizeof_u32(dst));

	CRYPTO_CONTEXT_RESET(c);
	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_DECRYPT, aes_mode);

	/* decipher the previous result and compare with original */

	LOG_INFO("Decipher data ciphered above and compare that the result matches original data\n");

	se_util_mem_set((uint8_t *)&arg, 0U, sizeof_u32(arg));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_DECRYPT;
	arg.ca_algo     = aes_mode;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	/* Clear keyslot AES_KEYSLOT
	 */
	ret = se_clear_device_aes_keyslot(SE_CDEV_SE0, AES_KEYSLOT);
	CCC_ERROR_CHECK(ret,
			LOG_ERROR("Failed to clear aes keyslot %u\n", AES_KEYSLOT));

	/* Preset the AES key and IV into AES_KEYSLOT.
	 * the PRESET IV does not affect AES-CTR mode (as it uses counter, not iV).
	 */
	ret = se_set_device_aes_keyslot_and_iv(SE_CDEV_SE0, AES_KEYSLOT,
			akey.k_aes_key, 128U, iv);
	CCC_ERROR_CHECK(ret);
	/* use the key preset by se_set_device_aes_keyslot_and_iv() */
	akey.k_flags |= KEY_FLAG_USE_KEYSLOT_KEY;

	if (aes_mode == TE_ALG_AES_CTR) {
		/* Use default: this is actually "counter increment" value 0 maps
		 * to standard increment 1.
		 */
		se_util_mem_set(arg.ca_init.aes.ctr_mode.ci_counter, 0U, 16U);
	} else {
		/* use the IV preset by se_set_device_aes_keyslot_and_iv() */
		arg.ca_init.aes.flags |= INIT_FLAG_CIPHER_USE_PRESET_IV;
	}

	arg.ca_set_key.kdata = &akey;

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
	CCC_ERROR_CHECK(ret);

	DUMP_HEX("DECIPHER RESULT:", dst, sizeof_u32(dst));

	VERIFY_ARRAY_VALUE(dst, data, sizeof_u32(data));

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}
#endif /* TEST_AES_IV_PRESET */

// could also do OFB mode with this function....

/* Encrypt stream cipher mode: 1..32 bytes in a loop verifying result and deciphering and verifying result
 * matches originl source data
 */
static status_t TEST_cipher_unaligned(crypto_context_t *c, te_crypto_algo_t aes_mode, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg;

	unsigned char data[32] = "abcdefghijklmnopabcdefghijklmnop";
	DMA_ALIGN_DATA uint8_t dst[32];
	uint32_t written = 0U;
	uint32_t inx = 0U;
	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = 16,
		.k_aes_key = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	};

	static const uint8_t correct[] = {
		0x07, 0x8b, 0x28, 0xb0, 0x8a, 0xec, 0x4b, 0x53, 0xe1, 0x26, 0x91, 0x35, 0xa7, 0x5a, 0x44, 0x5e,
		0x39, 0x80, 0x9f, 0xaa, 0x9f, 0x18, 0x57, 0x09, 0x5f, 0x15, 0x76, 0x3b, 0xc9, 0x89, 0x2a, 0x2a,
	};

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, aes_mode);

	if (aes_mode != TE_ALG_AES_CTR) {
		CCC_ERROR_WITH_ECODE(ERR_GENERIC,
				     LOG_ERROR("Only CTR mode supported for unaligned tests now: 0x%x\n",
					       ret));
	}

	// XX FIXME: Since steram ciphers do not grow the input => ctr mode zero length
	// XX  cipher makes no sense...
	//
	// Encrypt first, then verify result, then decrypt and verify result

	for (inx = 1; inx <= sizeof_u32(data); inx++) {

		// cipher

		se_util_mem_set(dst, 0U, sizeof_u32(dst));
		se_util_mem_set((uint8_t *)&arg, 0U, sizeof_u32(arg));

		/* Preset fields */
		arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
		arg.ca_algo     = aes_mode;
		arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

		LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
		arg.ca_init.engine_hint = eid;

		if (aes_mode == TE_ALG_AES_CTR) {
			arg.ca_init.aes.ctr_mode.ci_increment = 1;
			// Constant CTR mode preset value is a security issue if key is constant
			// but hey, this is just a test...
			se_util_mem_set(arg.ca_init.aes.ctr_mode.ci_counter, 0U, 16U);
		}

		arg.ca_set_key.kdata = &akey;

		arg.ca_data.src_size = inx;
		arg.ca_data.src = data;
		arg.ca_data.dst_size = sizeof_u32(dst);
		arg.ca_data.dst = dst;

		ret = CRYPTO_OPERATION(c, &arg);
		LOG_INFO("Operation used engine 0x%x (%s)\n",
			 arg.ca_init.engine_hint,
			 eid_name(arg.ca_init.engine_hint));
		LOG_INFO("CRYPTO(unaligned): SRC len %u, ALGO: 0x%x, ret %d, attached handle: %u\n",
			   inx, aes_mode, ret, arg.ca_handle);
		CCC_ERROR_CHECK(ret);

		// verify cipher result

		written = arg.ca_data.dst_size;
		LOG_INFO("0x%x algorithm input size %u output len %u\n", aes_mode, inx, written);
		DUMP_HEX("CIPHER RESULT:", dst, written);

		TRAP_ASSERT(written == inx);
		VERIFY_ARRAY_VALUE(dst, correct, inx);

		CRYPTO_CONTEXT_RESET(c);

		// decipher (DST contains the value to decipher, don't clear)

		CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_DECRYPT, aes_mode);

		se_util_mem_set((uint8_t *)&arg, 0U, sizeof_u32(arg));

		/* Preset fields */
		arg.ca_alg_mode = TE_ALG_MODE_DECRYPT;
		arg.ca_algo     = aes_mode;
		arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

		LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
		arg.ca_init.engine_hint = eid;

		if (aes_mode == TE_ALG_AES_CTR) {
			arg.ca_init.aes.ctr_mode.ci_increment = 1;
			// Constant CTR mode preset value is a security issue if key is constant
			// but hey, this is just a test...
			se_util_mem_set(arg.ca_init.aes.ctr_mode.ci_counter, 0U, 16U);
		}

		arg.ca_set_key.kdata = &akey;

		arg.ca_data.src_size = inx;
		arg.ca_data.src = dst;		// Decipher data from DST
		arg.ca_data.dst_size = sizeof_u32(dst);
		arg.ca_data.dst = dst;		// Back to DST

		ret = CRYPTO_OPERATION(c, &arg);
		LOG_INFO("Operation used engine 0x%x (%s)\n",
			 arg.ca_init.engine_hint,
			 eid_name(arg.ca_init.engine_hint));
		LOG_INFO("CRYPTO(unaligned): SRC len %u, ALGO: 0x%x, ret %d, attached handle: %u\n",
			   inx, aes_mode, ret, arg.ca_handle);
		CCC_ERROR_CHECK(ret);

		// verify decipher result

		written = arg.ca_data.dst_size;

		TRAP_ASSERT(written == inx);

		// Decipher data compared with original source data
		VERIFY_ARRAY_VALUE(dst, data, inx);
		DUMP_HEX("DECIPHER RESULT:", dst, written);
	}
fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

static status_t TEST_cipher_in_place(crypto_context_t *c, te_crypto_algo_t aes_mode, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	uint32_t written = 0U;
	static DMA_ALIGN_DATA uint8_t data[DMA_ALIGN_SIZE(64 + 16)];
	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = 16,
		.k_aes_key = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	};

	uint32_t data_len = 64U;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, aes_mode);

	se_util_mem_set(data, 0U, sizeof_u32(data));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = aes_mode;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	se_util_mem_set(arg.ca_init.aes.ci_iv, 0U, 16U);

	if (aes_mode == TE_ALG_AES_CTR) {
		arg.ca_init.aes.ctr_mode.ci_increment = 1U;
		// Constant CTR mode preset value is a security issue if key is constant
		// but hey, this is just a test...
		se_util_mem_set(arg.ca_init.aes.ctr_mode.ci_counter, 0U, 16U);
	}

	arg.ca_opcode = TE_OP_INIT;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	CCC_ERROR_CHECK(ret);

	arg.ca_set_key.kdata = &akey;
	arg.ca_opcode = TE_OP_SET_KEY;

	ret = CRYPTO_OPERATION(c, &arg);

	LOG_INFO("CRYPTO(set_key)[INPLACE] handle %u: ret %d\n",
		 arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	arg.ca_opcode = TE_OP_DOFINAL;

	arg.ca_data.src_size = data_len;
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(data);
	arg.ca_data.dst = data;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("CRYPTO(dofinal)[INPLACE] handle %u: ret %d\n",
		 arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	written = arg.ca_data.dst_size;
	LOG_INFO("CRYPTO(after dofinal)[INPLACE]: DST_SIZE %u\n", written);

	DUMP_HEX("CIPHER RESULT (src == dst):", data, written);

	se_util_mem_set((uint8_t *)&arg, 0U, sizeof_u32(arg));

	LOG_INFO("Deciphering data ciphered in-place: algo 0x%x\n", aes_mode);

	CRYPTO_CONTEXT_RESET(c);
	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_DECRYPT, aes_mode);

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_DECRYPT;
	arg.ca_algo     = aes_mode;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	/* pass the above the key params as well */
	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = written;
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(data);
	arg.ca_data.dst = data;

	ret = CRYPTO_OPERATION(c, &arg);
	CCC_ERROR_CHECK(ret);

	DUMP_HEX("AES decipher result:", data, arg.ca_data.dst_size);
	TRAP_ASSERT(arg.ca_data.dst_size == data_len);

	for (unsigned int i = 0; i < data_len; i++) {
		if (data[i] != 0x00) {
			CCC_ERROR_WITH_ECODE(ERR_GENERIC,
					     LOG_ERROR("Deciphered data not zero after cipher in place, decipher to dec: %u\n",
						       i));
		}
	}
	LOG_INFO("[ Decipher result verified zero ]\n");

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

static status_t TEST_cipher_with_update(crypto_context_t *c, te_crypto_algo_t aes_mode, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	unsigned char data[16] = "abcdefghijklmnop";
	DMA_ALIGN_DATA uint8_t dst[48];	// 3*16 bytes ciphered
	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = 16,
		.k_aes_key = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	};

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, aes_mode);

	se_util_mem_set(dst, 0U, sizeof_u32(dst));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = aes_mode;
	arg.ca_opcode   = op_INIT_KEY;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	// INIT fields
	if (aes_mode == TE_ALG_AES_CTR) {
		arg.ca_init.aes.ctr_mode.ci_increment = 1;
		se_util_mem_set(arg.ca_init.aes.ctr_mode.ci_counter, 0U, 16U);
	} else {
		se_util_mem_set(arg.ca_init.aes.ci_iv, 0U, 16);
	}

	// KEY
	arg.ca_set_key.kdata = &akey;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	LOG_INFO("CRYPTO(init|set_key) handle %u: ret %d\n",
		   arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	uint32_t dst_offset = 0U;
	uint32_t written = 0U;

	// FIRST block update

	arg.ca_opcode = TE_OP_UPDATE;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(dst) - dst_offset;
	arg.ca_data.dst = dst + dst_offset;

	ret = CRYPTO_OPERATION(c, &arg);
	CCC_ERROR_CHECK(ret);

	written = arg.ca_data.dst_size;
	LOG_INFO("After CRYPTO(update#1): written %u, offset %u\n", written, dst_offset);
	dst_offset += written;

	// SECOND block update

	arg.ca_opcode = TE_OP_UPDATE;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(dst) - dst_offset;
	arg.ca_data.dst = dst + dst_offset;

	ret = CRYPTO_OPERATION(c, &arg);
	CCC_ERROR_CHECK(ret);

	written = arg.ca_data.dst_size;
	LOG_INFO("After CRYPTO(update#2): written %u, offset %u\n", written, dst_offset);
	dst_offset += written;

	// THIRD  block dofinal

	arg.ca_opcode = op_FINAL_REL;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(dst) - dst_offset;
	arg.ca_data.dst = dst + dst_offset;

	ret = CRYPTO_OPERATION(c, &arg);
	CCC_ERROR_CHECK(ret);

	written = arg.ca_data.dst_size;
	dst_offset += written;
	LOG_INFO("After CRYPTO(dofinal): now written %u, total %u (equals sizeof_u32(dst) %u?)\n",
		   written, dst_offset, sizeof_u32(dst));

	// 48 bytes must have been written out

	TRAP_ASSERT(dst_offset == sizeof_u32(dst));

	DUMP_HEX("CIPHER RESULT (using updates):", dst, sizeof_u32(dst));

	// XXXXX VERIFY CORRECT RESULTS HERE!!!!! (juki: saturday)
fail:
	CRYPTO_CONTEXT_RESET(c);
	LOG_INFO("%s: ret: 0x%x\n", __func__,ret);
	return ret;
}

/* Test data from NIST 800-38a.pdf test vectors for AES-CTR-128
 *
 * Expected result:

 87 4d 61 91 b6 20 e3 26 1b ef 68 64 99 0d b6 ce
 98 06 f6 6b 79 70 fd ff 86 17 18 7b b9 ff fd ff
 5a e4 df 3e db d5 d3 5e 5b 4f 09 02 0d b0 3e ab
 1e 03 1d da 2f be 03 d1 79 21 70 a0 f3 00 9c ee

 Init. Counter  f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
 Block #1
 Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
 Output Block   ec8cdf7398607cb0f2d21675ea9ea1e4
 Plaintext      6bc1bee22e409f96e93d7e117393172a
 Ciphertext     874d6191b620e3261bef6864990db6ce
 Block #2
 Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff00
 Output Block   362b7c3c6773516318a077d7fc5073ae
 Plaintext      ae2d8a571e03ac9c9eb76fac45af8e51
 Ciphertext     9806f66b7970fdff8617187bb9fffdff
 Block #3
 Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff01
 Output Block   6a2cc3787889374fbeb4c81b17ba6c44
 Plaintext      30c81c46a35ce411e5fbc1191a0a52ef
 Ciphertext     5ae4df3edbd5d35e5b4f09020db03eab
 Block #4
 Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff02
 Output Block   e89c399ff0f198c6d40a31db156cabfe
 Plaintext      f69f2445df4f9b17ad2b417be66c3710
 Ciphertext     1e031dda2fbe03d1792170a0f3009cee
*/
static status_t TEST_cipher128_ctr_NIST(crypto_context_t *c, te_crypto_algo_t aes_mode, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	/* 64 bytes of AES-CTR-128 test data (4 aes blocks) */
	static const uint8_t data[ 4 * 16 ] = {
		0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
		0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
		0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
		0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
		0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
		0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
		0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
		0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10,
	};

	DMA_ALIGN_DATA uint8_t dst[sizeof_u32(data)];

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = 16,
		.k_aes_key = { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
			       0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c },
	};

	te_args_init_t init_setup = {
		.aes = {
			.ctr_mode = {
				.ci_counter = { 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
						0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff },
				.ci_increment = 1,
				// By default, driver code increments .ci_counter as big endian value
				// This is NIST compatible (native SE_HW uses as little endian => bad)
			},
		},
	};

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, aes_mode);

	if (aes_mode != TE_ALG_AES_CTR) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LOG_ERROR("This is a AES-CTR mode test; wrong mode 0x%x provide\n",
					       aes_mode));
	}

	se_util_mem_set(dst, 0U, sizeof_u32(dst));

	/* preset fields to encrypt with AES-CTR mode */
	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = TE_ALG_AES_CTR;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	/* setup */
	arg.ca_init = init_setup;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	/* provide key */
	arg.ca_set_key.kdata = &akey;

	/* and data */
	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	CCC_ERROR_CHECK(ret);

	DUMP_HEX("CIPHER AES-CTR-128 (NIST):", dst, arg.ca_data.dst_size);
fail:
	CRYPTO_CONTEXT_RESET(c);
	LOG_INFO("%s: ret: 0x%x\n", __func__,ret);
	return ret;
}

/*
  Init. Counter  f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
  Block #1
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
  Output Block   717d2dc639128334a6167a488ded7921
  Plaintext      6bc1bee22e409f96e93d7e117393172a
  Ciphertext     1abc932417521ca24f2b0459fe7e6e0b
  Block #2
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff00
  Output Block   a72eb3bb14a556734b7bad6ab16100c5
  Plaintext      ae2d8a571e03ac9c9eb76fac45af8e51
  Ciphertext     090339ec0aa6faefd5ccc2c6f4ce8e94
  Block #3
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff01
  Output Block   2efeae2d72b722613446dc7f4c2af918
  Plaintext      30c81c46a35ce411e5fbc1191a0a52ef
  Ciphertext     1e36b26bd1ebc670d1bd1d665620abf7
  Block #4
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff02
  Output Block   b9e783b30dd7924ff7bc9b97beaa8740
  Plaintext      f69f2445df4f9b17ad2b417be66c3710
  Ciphertext     4f78a7f6d29809585a97daec58c6b050
*/
static status_t TEST_cipher192_ctr_NIST(crypto_context_t *c, te_crypto_algo_t aes_mode, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	/* 64 bytes of AES-CTR-128 test data (4 aes blocks) */
	static const uint8_t data[] = {
		0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
		0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
		0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
		0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
		0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
		0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
		0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
		0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10,
	};

	DMA_ALIGN_DATA uint8_t dst[sizeof_u32(data)];

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = (192/8),
		.k_aes_key = { 0x8e, 0x73, 0xb0, 0xf7, 0xda, 0x0e, 0x64, 0x52,
			       0xc8, 0x10, 0xf3, 0x2b, 0x80, 0x90, 0x79, 0xe5,
			       0x62, 0xf8, 0xea, 0xd2, 0x52, 0x2c, 0x6b, 0x7b },
	};

	te_args_init_t init_setup = {
		.aes = {
			.ctr_mode = {
				.ci_counter = { 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
						0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff },
				.ci_increment = 1,
				// By default, driver code increments .ci_counter as big endian value
				// This is NIST compatible (native SE_HW uses as little endian => bad)
			},
		},
	};

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, aes_mode);

	if (aes_mode != TE_ALG_AES_CTR) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LOG_ERROR("This is a AES-CTR mode test; wrong mode 0x%x provide\n",
					       aes_mode));
	}

	se_util_mem_set(dst, 0U, sizeof_u32(dst));

	/* preset fields to encrypt with AES-CTR mode */
	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = TE_ALG_AES_CTR;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	/* setup */
	arg.ca_init = init_setup;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	/* provide key */
	arg.ca_set_key.kdata = &akey;

	/* and data */
	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	CCC_ERROR_CHECK(ret);

	DUMP_HEX("CIPHER AES-CTR-192 (NIST):", dst, arg.ca_data.dst_size);
fail:
	CRYPTO_CONTEXT_RESET(c);
	LOG_INFO("%s: ret: 0x%x\n", __func__,ret);
	return ret;
}

/*
  Init. Counter  f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
  Block #1
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
  Output Block   0bdf7df1591716335e9a8b15c860c502
  Plaintext      6bc1bee22e409f96e93d7e117393172a
  Ciphertext     601ec313775789a5b7a7f504bbf3d228
  Block #2
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff00
  Output Block   5a6e699d536119065433863c8f657b94
  Plaintext      ae2d8a571e03ac9c9eb76fac45af8e51
  Ciphertext     f443e3ca4d62b59aca84e990cacaf5c5
  Block #3
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff01
  Output Block   1bc12c9c01610d5d0d8bd6a3378eca62
  Plaintext      30c81c46a35ce411e5fbc1191a0a52ef
  Ciphertext     2b0930daa23de94ce87017ba2d84988d
  Block #4
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff02
  Output Block   2956e1c8693536b1bee99c73a31576b6
  Plaintext      f69f2445df4f9b17ad2b417be66c3710
  Ciphertext     dfc9c58db67aada613c2dd08457941a6
*/
static status_t TEST_cipher256_ctr_NIST(crypto_context_t *c, te_crypto_algo_t aes_mode, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	/* 64 bytes of AES-CTR-128 test data (4 aes blocks) */
	static const uint8_t data[] = {
		0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
		0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
		0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
		0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
		0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
		0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
		0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
		0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10,
	};

	DMA_ALIGN_DATA uint8_t dst[sizeof_u32(data)];

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = (256/8),
		.k_aes_key = { 0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
			       0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
			       0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
			       0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4 },
	};

	te_args_init_t init_setup = {
		.aes = {
			.ctr_mode = {
				.ci_counter = { 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
						0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff },
				.ci_increment = 1,
				// By default, driver code increments .ci_counter as big endian value
				// This is NIST compatible (native SE_HW uses as little endian => bad)
			},
		},
	};

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, aes_mode);

	if (aes_mode != TE_ALG_AES_CTR) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LOG_ERROR("This is a AES-CTR mode test; wrong mode 0x%x provide\n",
					       aes_mode));
	}

	se_util_mem_set(dst, 0U, sizeof_u32(dst));

	/* preset fields to encrypt with AES-CTR mode */
	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = TE_ALG_AES_CTR;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	/* setup */
	arg.ca_init = init_setup;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	/* provide key */
	arg.ca_set_key.kdata = &akey;

	/* and data */
	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	CCC_ERROR_CHECK(ret);

	DUMP_HEX("CIPHER AES-CTR-256 (NIST):", dst, arg.ca_data.dst_size);
fail:
	CRYPTO_CONTEXT_RESET(c);
	LOG_INFO("%s: ret: 0x%x\n", __func__,ret);
	return ret;
}

static status_t TEST_cipher_ctr_overflow(crypto_context_t *c, te_crypto_algo_t aes_mode, engine_id_t eid, uint32_t initial_bytes)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };
	uint32_t result_bytes = 0U;

	/* 64 bytes of AES-CTR-128 test data (4 aes blocks) */
	static const uint8_t data[] = {
		0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
		0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
		0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
		0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
		0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
		0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
		0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
		0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10,
	};

	DMA_ALIGN_DATA uint8_t dst[sizeof_u32(data)];

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = (192U/8U),
		.k_aes_key = { 0x8e, 0x73, 0xb0, 0xf7, 0xda, 0x0e, 0x64, 0x52,
			       0xc8, 0x10, 0xf3, 0x2b, 0x80, 0x90, 0x79, 0xe5,
			       0x62, 0xf8, 0xea, 0xd2, 0x52, 0x2c, 0x6b, 0x7b },
	};

	/* When 4 blocks are added => counter will overflow to the nonce
	 */
	te_args_init_t init_setup = {
		.aes = {
			.ctr_mode = {
				.ci_counter = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
						0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xFC },
				.ci_increment = 1,
				// By default, driver code increments .ci_counter as big endian value
				// This is NIST compatible (native SE_HW uses as little endian => bad)
			},
		},
	};

	if (initial_bytes > sizeof_u32(data)) {
		initial_bytes = sizeof_u32(data);
	}

	LOG_ALWAYS("AES-CTR overflow test, initial bytes %u out of %u\n",
		   initial_bytes, sizeof_u32(data));

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, aes_mode);

	if (aes_mode != TE_ALG_AES_CTR) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LOG_ERROR("This is a AES-CTR mode test; wrong mode 0x%x provide\n",
					       aes_mode));
	}

	se_util_mem_set(dst, 0U, sizeof_u32(dst));

	/* preset fields to encrypt with AES-CTR mode */
	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = TE_ALG_AES_CTR;
	arg.ca_opcode   = TE_OP_INIT | TE_OP_SET_KEY | TE_OP_UPDATE;

	/* setup */
	arg.ca_init = init_setup;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	/* provide key */
	arg.ca_set_key.kdata = &akey;

	/* and data
	 * First pass initial_bytes, then 64-initial_bytes
	 * and the counter overflow should be trapped
	 */
	arg.ca_data.src_size = initial_bytes;
	arg.ca_data.src = &data[0];
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	DUMP_HEX("Intermediate AES-CTR counter:", &init_setup.aes.ctr_mode.ci_counter[0], 16U);
	CCC_ERROR_CHECK(ret);

	result_bytes = arg.ca_data.dst_size;

	/* Next operation completes the AES-CTR
	 */
	arg.ca_opcode = TE_OP_DOFINAL;

	/* pass 64-(initial_chunks*16) of data not yet passed
	 * and the counter overflow should be trapped
	 */
	arg.ca_data.src_size = sizeof_u32(data) - initial_bytes;
	arg.ca_data.src = &data[initial_bytes];
	arg.ca_data.dst_size = sizeof_u32(dst) - result_bytes;
	arg.ca_data.dst = &dst[result_bytes];

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	DUMP_HEX("Resulting AES-CTR counter:", &init_setup.aes.ctr_mode.ci_counter[0], 16U);

	CCC_ERROR_CHECK(ret);

	CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
			     LOG_ERROR("AES-CTR overflow not detected"));
fail:
	CRYPTO_CONTEXT_RESET(c);
	LOG_INFO("%s: ret: 0x%x\n", __func__,ret);
	return ret;
}

#if HAVE_SE_ASYNC_AES
static status_t TEST_async_cipher_with_update(crypto_context_t *c, te_crypto_algo_t aes_mode, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg;
	uint32_t checks = 0U;
	bool done = false;
	uint32_t inx = 0U;

	unsigned char data[64] = "abcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnop";
	DMA_ALIGN_DATA unsigned char dst[64];
	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = 16,
		.k_aes_key = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	};

	// echo -n "abcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnop" | openssl enc -aes-128-ctr -K 0 -iv 0 | xxd -i -c 16
	static const uint8_t correct_ctr[] = {
		0x07, 0x8b, 0x28, 0xb0, 0x8a, 0xec, 0x4b, 0x53, 0xe1, 0x26, 0x91, 0x35, 0xa7, 0x5a, 0x44, 0x5e,
		0x39, 0x80, 0x9f, 0xaa, 0x9f, 0x18, 0x57, 0x09, 0x5f, 0x15, 0x76, 0x3b, 0xc9, 0x89, 0x2a, 0x2a,
		0x62, 0xea, 0xb9, 0xaa, 0x05, 0xd0, 0xc4, 0xfa, 0x9a, 0x42, 0xa9, 0xd5, 0x1c, 0xdc, 0x91, 0x08,
		0x96, 0xf7, 0xc9, 0xcf, 0x2c, 0x2d, 0x3e, 0x4b, 0x9e, 0x97, 0xe2, 0x93, 0xf9, 0xe5, 0xae, 0x90
	};

	// echo -n "abcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnop" | openssl enc -aes-128-cbc -nopad -K 0 -iv 0 | xxd -i -c 16
	static const uint8_t correct_cbc_nopad[] = {
		0xc3, 0xaf, 0x71, 0xad, 0xdf, 0xe4, 0xfc, 0xac, 0x69, 0x41, 0x28, 0x6a, 0x76, 0xdd, 0xed, 0xc2,
		0x2f, 0x97, 0xc4, 0x5d, 0xc2, 0x6b, 0x32, 0x22, 0x46, 0x36, 0x8c, 0xd2, 0x4c, 0xca, 0xbf, 0x54,
		0x00, 0xdc, 0x9a, 0xc5, 0x04, 0xa3, 0xd3, 0x2b, 0xfa, 0x2b, 0xda, 0x66, 0xa6, 0x45, 0x03, 0x14,
		0x25, 0x27, 0x4d, 0xee, 0x5b, 0xaa, 0xdd, 0x68, 0x0f, 0xdb, 0x8e, 0x83, 0x8f, 0xb4, 0xc6, 0x34
	};

	const uint8_t *correct = NULL;

	switch (aes_mode) {
	case TE_ALG_AES_CTR:
		correct = correct_ctr;
		break;
	case TE_ALG_AES_CBC_NOPAD:
		correct = correct_cbc_nopad;
		break;
	default:
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, aes_mode);

	se_util_mem_set((uint8_t *)&arg, 0, sizeof(arg));
	se_util_mem_set((uint8_t *)dst, 0, sizeof(dst));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = aes_mode;
	arg.ca_opcode   = op_INIT_KEY;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	// INIT fields
	if (aes_mode == TE_ALG_AES_CTR) {
		arg.ca_init.aes.ctr_mode.ci_increment = 1;
		se_util_mem_set(arg.ca_init.aes.ctr_mode.ci_counter, 0, 16);
	} else {
		se_util_mem_set(arg.ca_init.aes.ci_iv, 0, 16);
	}

	// KEY
	arg.ca_set_key.kdata = &akey;

	ret = CRYPTO_OPERATION(c, &arg);
	CCC_ERROR_CHECK(ret);

	LOG_INFO("CRYPTO(init|set_key) handle %u: ret %d\n",
		 arg.ca_handle,ret);

	/* Pass the input as 4 chunks of 16 bytes each.
	 *
	 * First three in ASYNC UPDATE and the last as ASYNC DOFINAL
	 */
	for (inx = 0U; inx <= 3U; inx++) {
		if (3U == inx) {
			arg.ca_opcode = TE_OP_ASYNC_DOFINAL_START;
		} else {
			arg.ca_opcode = TE_OP_ASYNC_UPDATE_START;
		}

		arg.ca_data.src_size = 16U;
		arg.ca_data.src = &data[inx*16U];

		arg.ca_data.dst_size = 16U;
		arg.ca_data.dst = &dst[inx*16U];

		ret = CRYPTO_OPERATION(c, &arg);
		CCC_ERROR_CHECK(ret);

		/* The engine state is placed to this init field by the check state call.
		 * Using a CCC macro to fetch it for readability.
		 */
		arg.ca_init.async_state = 0xDEADBEEF;	// Make sure there is some value which traps unless syscall works

		/* Non-blocking STATE check is optional, can also call the blocking
		 * TE_OP_ASYNC_FINISH without polling.
		 */
		done = false;
		checks = 0;
		while(! done) {
			checks++;
			arg.ca_opcode = TE_OP_ASYNC_CHECK_STATE;
			ret = CRYPTO_OPERATION(c, &arg);
			CCC_ERROR_CHECK(ret,
					CCC_ERROR_MESSAGE("CHECK STATE: checks %u\n", checks));

			switch (ASYNC_GET_SE_ENGINE_STATE(&arg)) {
			case ASYNC_CHECK_ENGINE_NONE:
				CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
						     LOG_ERROR("update:start => No operation ongoing\n"));
			case  ASYNC_CHECK_ENGINE_BUSY:
				LOG_INFO("update:start => SE engine is busy\n");
				continue;
			case ASYNC_CHECK_ENGINE_IDLE:
				LOG_INFO("update:start => SE engine is IDLE\n");
				done = true;
				break;
			default:
				CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
						     LOG_ERROR("update:start => bad state value: 0x%x\n",
							       ASYNC_GET_SE_ENGINE_STATE(&arg)));
			}
		}

		arg.ca_opcode = TE_OP_ASYNC_FINISH;

		ret = CRYPTO_OPERATION(c, &arg);
		CCC_ERROR_CHECK(ret,
				LOG_INFO("CIPHER(dofinal:start return) handle %u: ret %d\n",
					 arg.ca_handle,ret));
	}


	DUMP_HEX("CIPHER RESULT (using updates):", dst, sizeof_u32(dst));

	VERIFY_ARRAY_VALUE(dst, correct, sizeof_u32(dst));

	CRYPTO_CONTEXT_RESET(c);

	/* Follow ENCRYPT by a single ASYNC DECRYPT of the result, in place.
	 */
	correct = (const uint8_t *)data;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_DECRYPT, aes_mode);

	se_util_mem_set((uint8_t *)&arg, 0, sizeof(arg));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_DECRYPT;
	arg.ca_algo     = aes_mode;
	arg.ca_opcode   = op_INIT_KEY;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	// INIT fields
	if (aes_mode == TE_ALG_AES_CTR) {
		arg.ca_init.aes.ctr_mode.ci_increment = 1U;
		se_util_mem_set(arg.ca_init.aes.ctr_mode.ci_counter, 0, 16U);
	} else {
		se_util_mem_set(arg.ca_init.aes.ci_iv, 0U, 16U);
	}

	// KEY
	arg.ca_set_key.kdata = &akey;

	ret = CRYPTO_OPERATION(c, &arg);
	CCC_ERROR_CHECK(ret);

	LOG_INFO("CRYPTO(init|set_key) handle %u: ret %d\n",
		 arg.ca_handle,ret);

	/* Pass the input as one 64 byte chunk to a single ASYNC AES dofinal.
	 *
	 * Decipher IN PLACE (dst = DECIPHER(dst))
	 */
	arg.ca_opcode = TE_OP_ASYNC_DOFINAL_START;

	arg.ca_data.src_size = 64U;
	arg.ca_data.src = &dst[0];

	arg.ca_data.dst_size = 64U;
	arg.ca_data.dst = &dst[0];

	ret = CRYPTO_OPERATION(c, &arg);
	CCC_ERROR_CHECK(ret);

	/* The engine state is placed to this init field by the check state call.
	 * Using a CCC macro to fetch it for readability.
	 */
	arg.ca_init.async_state = 0xDEADBEEF;	// Make sure there is some value which traps unless syscall works

	/* Non-blocking STATE check is optional, can also call the blocking
	 * TE_OP_ASYNC_FINISH without polling.
	 */
	done = false;
	checks = 0;
	while(! done) {
		checks++;
		arg.ca_opcode = TE_OP_ASYNC_CHECK_STATE;
		ret = CRYPTO_OPERATION(c, &arg);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("CHECK STATE: checks %u\n", checks));

		switch (ASYNC_GET_SE_ENGINE_STATE(&arg)) {
		case ASYNC_CHECK_ENGINE_NONE:
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
					     LOG_ERROR("dofinal:start => No operation ongoing\n"));
		case  ASYNC_CHECK_ENGINE_BUSY:
			LOG_INFO("dofinal:start => SE engine is busy\n");
			continue;
		case ASYNC_CHECK_ENGINE_IDLE:
			LOG_INFO("dofinal:start => SE engine is IDLE\n");
			done = true;
			break;
		default:
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
					     LOG_ERROR("dofinal:start => bad state value: 0x%x\n",
						       ASYNC_GET_SE_ENGINE_STATE(&arg)));
		}
	}

	arg.ca_opcode = TE_OP_ASYNC_FINISH;

	ret = CRYPTO_OPERATION(c, &arg);
	CCC_ERROR_CHECK(ret,
			LOG_INFO("DECIPHER(dofinal:start return) handle %u: ret %d\n",
				 arg.ca_handle,ret));

	DUMP_HEX("DECIPHER RESULT (using updates):", dst, sizeof_u32(dst));

	VERIFY_ARRAY_VALUE(dst, correct, sizeof_u32(dst));

fail:
	CRYPTO_CONTEXT_RESET(c);
	LOG_INFO("%s: ret: 0x%x\n", __func__,ret);
	return ret;
}
#endif /* HAVE_SE_ASYNC_AES */

#ifdef TEST_AES_IV_PRESET
#ifdef TEST_SRS_VERIFY
static const uint8_t key256_clr_keyslot_plaintext[] = {
	0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t key256_clr_keyslot_ciphertext[] = {
	0xb1, 0xf4, 0x06, 0x6e, 0x6f, 0x4f, 0x18, 0x7d,
	0xfe, 0x5f, 0x2a, 0xd1, 0xb1, 0x78, 0x19, 0xd0,
};

static status_t aes_keyslot_clr_verify(crypto_context_t *c,
				    te_crypto_alg_mode_t mode, engine_id_t engine_id,
				    uint32_t keybytes, const uint8_t *input, const uint8_t *output,
				    uint32_t data_size, bool use_keyslot_key)
{
	status_t ret = NO_ERROR;
	DMA_ALIGN_DATA uint8_t result[64U];
	te_crypto_args_t arg = { .ca_handle = 0U, };

	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
	};

	if (NULL == c) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	CRYPTO_CONTEXT_SETUP(c, mode, TE_ALG_AES_ECB_NOPAD);
	CCC_ERROR_CHECK(ret);

	se_util_mem_set(result, 0, sizeof_u32(result));

	arg.ca_algo     = TE_ALG_AES_ECB_NOPAD;
	arg.ca_alg_mode = mode;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;
	arg.ca_init.engine_hint = engine_id;

	akey.k_byte_size = keybytes;
	akey.k_keyslot = AES_KEYSLOT;

	if (BOOL_IS_TRUE(use_keyslot_key)) {
		akey.k_flags = KEY_FLAG_USE_KEYSLOT_KEY | KEY_FLAG_LEAVE_KEY_IN_KEYSLOT;
	} else {
		se_util_mem_set(akey.k_aes_key, 0, akey.k_byte_size);
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
				LOG_ERROR("%s: result compare failed; ALGO: 0x%x, MODE: %u\n",
					__func__, TE_ALG_AES_ECB_NOPAD, mode));
	}

fail:
	CRYPTO_CONTEXT_RESET(c);

	return ret;
}

static status_t TEST_se_aes_clr_keyslot_verify(crypto_context_t *c, te_crypto_algo_t algo, engine_id_t eid)
{
	status_t ret = NO_ERROR;

	LOG_ALWAYS("CCC keyslot clearing verification test.\n");

	/* Verfiy the CCC default behavior where AES keyslots are cleared after
	 * AES crypto operation AES-CTR
	 */

	/* Regular AES cipher test without setting flag LEAVE_KEY_IN_KEYSLOT */
	TEST_cipher256_ctr_NIST(c, algo, CCC_ENGINE_ANY);
	CCC_ERROR_CHECK(ret);

	/**
	 * The following 2 cases: one uses keyslot key (supposed all-zeroed) to
	 * encrypt, the other uses key argument (all-zeroed) to encrypt.
	 * The 2 contrary cases prove the keyslot has been cleared to zero.
	 */

	/* Use keyslot preset key (supposed to be all-zeroed) to encrypt */
	ret = aes_keyslot_clr_verify(c, TE_ALG_MODE_ENCRYPT, eid, 32U,
			key256_clr_keyslot_plaintext, key256_clr_keyslot_ciphertext, 16U, true);
	CCC_ERROR_CHECK(ret);

	/* Use all-zeroed key from key argumenmt to conduct the same encryption */
	ret = aes_keyslot_clr_verify(c, TE_ALG_MODE_ENCRYPT, eid, 32U,
			key256_clr_keyslot_plaintext, key256_clr_keyslot_ciphertext, 16U, false);
	CCC_ERROR_CHECK(ret);

fail:
	return ret;
}
#endif /* TEST_SRS_VERIFY */
#endif /* TEST_AES_IV_PRESET */

status_t run_cipher_test_cases(crypto_context_t *crypto_ctx)
{
	status_t ret = NO_ERROR;

#ifdef NIST_AES_TESTS
	TEST_SUITE("NIST-aes-tests (SE0 AES0 engine)", run_NIST_aes_tests(crypto_ctx, CCC_ENGINE_SE0_AES0));
	TEST_SUITE("NIST-aes-tests (SE0 AES1 engine)", run_NIST_aes_tests(crypto_ctx, CCC_ENGINE_SE0_AES1));

#ifdef TEST_AES_XTS
	TEST_SUITE("aes-xts-tests (SE0 AES1 engine)", run_aes_xts_tests(crypto_ctx, CCC_ENGINE_ANY));
#endif
#endif /* NIST_AES_TESTS */

	TEST_ERRCHK(TEST_cipher_in_place, CCC_ENGINE_ANY, TE_ALG_AES_CBC_NOPAD);
	TEST_ERRCHK(TEST_cipher_in_place, CCC_ENGINE_ANY, TE_ALG_AES_ECB_NOPAD);
	TEST_ERRCHK(TEST_cipher_in_place, CCC_ENGINE_ANY, TE_ALG_AES_CBC);	/* PKCS#7 padding */
	TEST_ERRCHK(TEST_cipher_in_place, CCC_ENGINE_ANY, TE_ALG_AES_ECB);	/* PKCS#7 padding */

	TEST_ERRCHK(TEST_cipher_unaligned, CCC_ENGINE_ANY, TE_ALG_AES_CTR);

	TEST_ERRCHK(TEST_cipher_with_update, CCC_ENGINE_ANY, TE_ALG_AES_CTR);

	TEST_ERRCHK(TEST_cipher128_ctr_NIST, CCC_ENGINE_ANY, TE_ALG_AES_CTR); /* NIST AES128-CTR test vector */
	TEST_ERRCHK(TEST_cipher192_ctr_NIST, CCC_ENGINE_ANY, TE_ALG_AES_CTR); /* NIST AES192-CTR test vector */
	TEST_ERRCHK(TEST_cipher256_ctr_NIST, CCC_ENGINE_ANY, TE_ALG_AES_CTR); /* NIST AES256-CTR test vector */

	TEST_ERRCHK(TEST_cipher, CCC_ENGINE_ANY, TE_ALG_AES_CTR);

	/* AES-CTR 64 bit counter overflow detection test  */
	TEST_EXPECT_RET(TEST_cipher_ctr_overflow, ERR_TOO_BIG, CCC_ENGINE_ANY, TE_ALG_AES_CTR, 64U);
	TEST_EXPECT_RET(TEST_cipher_ctr_overflow, ERR_TOO_BIG, CCC_ENGINE_ANY, TE_ALG_AES_CTR, 32U);
	TEST_EXPECT_RET(TEST_cipher_ctr_overflow, ERR_TOO_BIG, CCC_ENGINE_ANY, TE_ALG_AES_CTR, 16U);
	TEST_EXPECT_RET(TEST_cipher_ctr_overflow, ERR_TOO_BIG, CCC_ENGINE_ANY, TE_ALG_AES_CTR, 1U);

	TEST_ERRCHK(TEST_cipher_with_update, CCC_ENGINE_ANY, TE_ALG_AES_CBC_NOPAD);

	TEST_ERRCHK(TEST_cipher, CCC_ENGINE_ANY, TE_ALG_AES_ECB_NOPAD);
	TEST_ERRCHK(TEST_cipher, CCC_ENGINE_ANY, TE_ALG_AES_CBC_NOPAD);
#ifdef TEST_AES_IV_PRESET
	TEST_ERRCHK(TEST_cipher_with_preset_key_iv, CCC_ENGINE_ANY, TE_ALG_AES_CBC_NOPAD);
#ifdef TEST_SRS_VERIFY
	/* Integration tests for verifying CCC AES keyslot clearing */
	TEST_ERRCHK(TEST_se_aes_clr_keyslot_verify, CCC_ENGINE_SE0_AES0, TE_ALG_AES_CTR);
	TEST_ERRCHK(TEST_se_aes_clr_keyslot_verify, CCC_ENGINE_SE0_AES1, TE_ALG_AES_CTR);
#endif
#endif
#if HAVE_AES_OFB
	TEST_ERRCHK(TEST_cipher, CCC_ENGINE_ANY, TE_ALG_AES_OFB);
#endif

#if ASYNC_TEST
	/* XXX make XTS tests also with async calls. */
	TEST_SUITE("NIST-aes-tests-async (SE0 AES0 engine)", run_NIST_async_aes_tests(crypto_ctx, CCC_ENGINE_SE0_AES0));
	TEST_SUITE("NIST-aes-tests-async (SE0 AES1 engine)", run_NIST_async_aes_tests(crypto_ctx, CCC_ENGINE_SE0_AES1));

#endif /* ASYNC_TEST */

#if HAVE_AES_CCM
	TEST_SUITE("AES-CCM tests", run_aes_ccm_tests(crypto_ctx, CCC_ENGINE_ANY));
#endif /* HAVE_AES_CCM */

#if HAVE_AES_GCM
	/* AES-GCM tests (encrypt, decrypt, with and without AAD) */
	TEST_SUITE("aes-gcm-tests (SE0 AES0 engine)", run_aes_gcm_tests(crypto_ctx, CCC_ENGINE_SE0_AES0));
	TEST_SUITE("aes-gcm-tests (SE0 AES1 engine)", run_aes_gcm_tests(crypto_ctx, CCC_ENGINE_SE0_AES1));
#endif

#if HAVE_SE_ASYNC_AES
	TEST_ERRCHK(TEST_async_cipher_with_update, CCC_ENGINE_ANY, TE_ALG_AES_CBC_NOPAD);
	TEST_ERRCHK(TEST_async_cipher_with_update, CCC_ENGINE_ANY, TE_ALG_AES_CTR);
#endif /* HAVE_SE_ASYNC_AES */

fail:
	return ret;
}
#endif /* HAVE_SE_AES */

#endif /* KERNEL_TEST_MODE */

