/*
 * Copyright (c) 2019-2021, LWPU CORPORATION. All rights reserved
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

/* Defined nonzero to run the kernel mode crypto tests
 */
#if KERNEL_TEST_MODE

#ifdef TEST_RECIPROCAL

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

/* Misleading name; this does not test a hash.
 */

#define MAX_HASH_SIZE 64U

static status_t do_digest(crypto_context_t *c, te_crypto_args_t *arg,
			  te_crypto_algo_t digest,
			  const uint8_t *data, uint32_t data_len,
			  uint8_t *hash, uint32_t hlen)
{
	status_t ret = NO_ERROR;

	se_util_mem_set((uint8_t *)c, 0U, sizeof_u32(crypto_context_t));
	se_util_mem_set((uint8_t *)arg, 0U, sizeof_u32(te_crypto_args_t));

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_DIGEST, digest);

	arg->ca_alg_mode = TE_ALG_MODE_DIGEST;
	arg->ca_algo     = digest;
	arg->ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg->ca_init.engine_hint = CCC_ENGINE_ANY;

	arg->ca_data.src_size = data_len;
	arg->ca_data.src      = data;
	arg->ca_data.dst_size = hlen;
	arg->ca_data.dst      = hash;

	ret = CRYPTO_OPERATION(c, arg);
	CCC_ERROR_CHECK(ret, LOG_ERROR("digest failed\n"));

	if (hlen != arg->ca_data.dst_size) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS, LOG_ERROR("Hash length mismatch\n"));
	}
fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

#if 0
/* The only requested algorithm is CMAC-AES.
 * That returns a 16 byte response always.
 *
 * Key object is passed in by the caller.
 */
static status_t do_cmac(crypto_context_t *c, te_crypto_args_t *arg,
			te_crypto_algo_t algo, te_args_key_data_t *akey,
			engine_id_t *eid,
			const uint8_t *data, uint32_t data_len,
			uint8_t *mac, uint32_t mlen)
{
	status_t ret = NO_ERROR;

	se_util_mem_set((uint8_t *)c, 0U, sizeof_u32(crypto_context_t));
	se_util_mem_set((uint8_t *)arg, 0U, sizeof_u32(te_crypto_args_t));
	se_util_mem_set(mac, 0U, mlen);

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, algo);

	arg->ca_alg_mode = TE_ALG_MODE_MAC;
	arg->ca_algo     = algo;
	arg->ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg->ca_init.engine_hint = eid;

	arg->ca_data.src_size = data_len;
	arg->ca_data.src      = data;
	arg->ca_data.dst_size = mac;
	arg->ca_data.dst       = mlen;

	arg->ca_set_key.kdata = akey;

	ret = CRYPTO_OPERATION(c, arg);
	CCC_ERROR_CHECK(ret, LOG_ERROR("mac failed\n"));

	if (mlen != arg->ca_data.dst_size) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS, LOG_ERROR("Mac length mismatch\n"));
	}
fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}
#endif /* 0 */

#define RCSUM_CIPHER	true
#define RCSUM_DECIPHER	false

static status_t do_cipher(crypto_context_t *c, te_crypto_args_t *arg,
			  te_args_key_data_t *akey, engine_id_t eid,
			  te_crypto_algo_t algo, bool encrypt,
			  const uint8_t *src, uint32_t src_len,
			  uint8_t *dst, uint32_t dst_len)
{
	status_t ret = NO_ERROR;
	te_crypto_alg_mode_t mode = TE_ALG_MODE_DECRYPT;

	se_util_mem_set((uint8_t *)c, 0U, sizeof_u32(crypto_context_t));
	se_util_mem_set((uint8_t *)arg, 0U, sizeof_u32(te_crypto_args_t));
	se_util_mem_set(dst, 0U, dst_len);

	if (encrypt) {
		mode = TE_ALG_MODE_ENCRYPT;
	}

	CRYPTO_CONTEXT_SETUP(c, mode, algo);

	arg->ca_alg_mode = mode;
	arg->ca_algo     = algo;
	arg->ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg->ca_init.engine_hint = eid;

	arg->ca_data.src_size = src_len;
	arg->ca_data.src      = src;
	arg->ca_data.dst_size = dst_len;
	arg->ca_data.dst      = dst;

	arg->ca_set_key.kdata = akey;

	ret = CRYPTO_OPERATION(c, arg);
	CCC_ERROR_CHECK(ret, LOG_ERROR("AES failed\n"));

	if (src_len != arg->ca_data.dst_size) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS, LOG_ERROR("AES length mismatch\n"));
	}
fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

#define RCSUM_DATA_SIZE 17U
#define AES_KEYSLOT     5U

/* Verification magically named as "reciprocal hash test".
 *
 * Does not really check that hashing works, unless the result
 * is also verified; that needs to be passed in or
 * callwlated by some other system.
 *
 * Uses: SHA-256 for digest, AES-CTR for cipher/decipher.
 */
static status_t TEST_reciprocal_hash(crypto_context_t *c,
				     engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg;

	te_crypto_algo_t digest = TE_ALG_SHA256;
	uint32_t hash_size = 32U;

	te_crypto_algo_t cipher = TE_ALG_AES_CTR;

	const uint8_t ptext[RCSUM_DATA_SIZE] = "Where is the cat?";
	uint8_t ctext[RCSUM_DATA_SIZE] = { 0x0U };
	uint8_t data[RCSUM_DATA_SIZE] = { 0x0U };

	uint8_t hash[MAX_HASH_SIZE] = { 0xDE, 0xAD, 0xBE, 0xEF, };
	uint8_t dhash[MAX_HASH_SIZE] = { 0x0U };

	static te_args_key_data_t akey = {
		.k_key_type  = KEY_TYPE_AES,
		.k_flags     = KEY_FLAG_PLAIN,
		.k_keyslot   = AES_KEYSLOT,
		.k_byte_size = 16U,
		.k_aes_key = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	};

	ret = do_digest(c, &arg, digest,
			ptext, sizeof_u32(ptext),
			hash, hash_size);
	CCC_ERROR_CHECK(ret);

	ret = do_cipher(c, &arg, &akey, eid,
			cipher, RCSUM_CIPHER,
			ptext, sizeof_u32(ptext),
			ctext, sizeof_u32(ctext));
	CCC_ERROR_CHECK(ret);

	se_util_mem_set(data, 0, RCSUM_DATA_SIZE);

	ret = do_cipher(c, &arg, &akey, eid,
			cipher, RCSUM_DECIPHER,
			ctext, sizeof_u32(ctext),
			data, sizeof_u32(data));
	CCC_ERROR_CHECK(ret);

	ret = do_digest(c, &arg, digest,
			data, sizeof_u32(data),
			dhash, hash_size);
	CCC_ERROR_CHECK(ret);

	if (! se_util_vec_match(hash, dhash, hash_size, false, &ret)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID,
				     LOG_ERROR("Hash mismatch\n"));
	}
	CCC_ERROR_CHECK(ret, LOG_ERROR("Hash compare error\n"));

	DUMP_HEX("DIGEST RESULT (ptext):", hash, hash_size);
	DUMP_HEX("DIGEST RESULT (data):",  dhash, hash_size);

	LOG_ALWAYS("[ Reciprocal hash test ok ]\n");
fail:
	return ret;
}

status_t run_reciprocal_hash_test(crypto_context_t *crypto_ctx, engine_id_t engine_id);

status_t run_reciprocal_hash_test(crypto_context_t *crypto_ctx, engine_id_t engine_id)
{
	return TEST_reciprocal_hash(crypto_ctx, engine_id);
}
#endif /* TEST_RECIPROCAL */

#endif /* KERNEL_TEST_MODE */

