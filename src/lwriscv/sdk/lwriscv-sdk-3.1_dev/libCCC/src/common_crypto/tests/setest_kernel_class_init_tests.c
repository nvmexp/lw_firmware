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

#include <crypto_random.h>
#include <crypto_derive.h>
#include <crypto_mac.h>
#include <crypto_md.h>
#include <crypto_acipher.h>
#include <crypto_asig.h>
#include <crypto_cipher.h>
#include <crypto_ae.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if !defined(HAVE_CCC_KDF)
#define HAVE_CCC_KDF (CCC_WITH_ECDH || CCC_WITH_X25519 || HAVE_SE_KAC_KDF || HAVE_SE_AES_KDF)
#endif /* !defined(HAVE_CCC_KDF) */

/* Test the classt initial program according to class.
 * Besids to test init functions, we can test with wrong input parameter alg to reach the
 * error handling in init function, that improve function-call coverage.
 */
static status_t TEST_class_init_function(crypto_context_t *c, te_crypto_algo_t alg, engine_id_t eid, te_crypto_class_t class)
{
	status_t ret = NO_ERROR;
	te_crypto_alg_mode_t mode;
	te_crypto_args_t arg = { .ca_handle = 0U, };
	static const struct crypto_class_funs_s random_cfuns = {
		.cfun_class	    = TE_CLASS_RANDOM,
	};
#if HAVE_CCC_KDF
	static const struct crypto_class_funs_s derive_cfuns = {
		.cfun_class	    = TE_CLASS_KEY_DERIVATION,
	};
#endif  /* HAVE_CCC_KDF */
	static const struct crypto_class_funs_s mac_cfuns = {
		.cfun_class	    = TE_CLASS_MAC,
	};
	static const struct crypto_class_funs_s digest_cfuns = {
		.cfun_class	    = TE_CLASS_DIGEST,
	};
	static const struct crypto_class_funs_s acipher_cfuns = {
		.cfun_class	    = TE_CLASS_ASYMMETRIC_CIPHER,
	};
	static const struct crypto_class_funs_s asig_cfuns = {
		.cfun_class	    = TE_CLASS_ASYMMETRIC_SIGNATURE,
	};
	static const struct crypto_class_funs_s cipher_cfuns = {
		.cfun_class	    = TE_CLASS_CIPHER,
	};
#if HAVE_AES_AEAD
	static const struct crypto_class_funs_s ae_cfuns = {
		.cfun_class	    = TE_CLASS_AE,
	};
#endif /* HAVE_AES_AEAD */

	se_util_mem_set((uint8_t *)c, 0U, sizeof_u32(crypto_context_t));

	switch (class) {
	case TE_CLASS_RANDOM:
		mode = TE_ALG_MODE_RANDOM;
		c->ctx_class = TE_CLASS_RANDOM;
		c->ctx_funs = &random_cfuns;
		break;
#if HAVE_CCC_KDF
	case TE_CLASS_KEY_DERIVATION:
		mode = TE_ALG_MODE_DERIVE;
		c->ctx_class = TE_CLASS_KEY_DERIVATION;
		c->ctx_funs = &derive_cfuns;
		arg.ca_init.ec.flags    = INIT_FLAG_EC_NONE;
		arg.ca_init.ec.lwrve_id = TE_LWRVE_NIST_P_192;
		arg.ca_init.engine_hint = eid;
		break;
#endif /* HAVE_CCC_KDF */
	case TE_CLASS_MAC:
		mode = TE_ALG_MODE_MAC;
		c->ctx_class = TE_CLASS_MAC;
		c->ctx_funs = &mac_cfuns;
		break;
	case TE_CLASS_DIGEST:
		mode = TE_ALG_MODE_DIGEST;
		c->ctx_class = TE_CLASS_DIGEST;
		c->ctx_funs = &digest_cfuns;
		break;
	case TE_CLASS_ASYMMETRIC_CIPHER:
		mode = TE_ALG_MODE_ENCRYPT;
		c->ctx_class = TE_CLASS_ASYMMETRIC_CIPHER;
		c->ctx_funs = &acipher_cfuns;
		break;
	case TE_CLASS_ASYMMETRIC_SIGNATURE:
		mode = TE_ALG_MODE_SIGN;
		c->ctx_class = TE_CLASS_ASYMMETRIC_SIGNATURE;
		c->ctx_funs = &asig_cfuns;
		break;
	case TE_CLASS_CIPHER:
		mode = TE_ALG_MODE_ENCRYPT;
		c->ctx_class = TE_CLASS_CIPHER;
		c->ctx_funs = &cipher_cfuns;
		break;
#if HAVE_AES_AEAD
	case TE_CLASS_AE:
		mode = TE_ALG_MODE_ENCRYPT;
		c->ctx_class = TE_CLASS_AE;
		c->ctx_funs = &ae_cfuns;
		break;
#endif /* HAVE_AES_AEAD */
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	c->ctx_handle = 0;
	c->ctx_handle_state = TE_HANDLE_FLAG_RESET;
	c->ctx_algo = alg;
	c->ctx_alg_mode = mode;
	c->ctx_app_index = 0;
	c->ctx_domain = TE_CRYPTO_DOMAIN_KERNEL;
	c->ctx_magic = CRYPTO_CONTEXT_MAGIX;

	arg.ca_alg_mode = mode;
	arg.ca_algo     = alg;
	arg.ca_opcode   = TE_OP_INIT | TE_OP_RESET; //reset context after INIT failed

	LOG_INFO("Hint: use engine 0x%x (%s) for random 0x%x\n", eid, eid_name(eid), alg);
	arg.ca_init.engine_hint = eid;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	LOG_INFO("Class 0x%x (TE_OP_INIT => RETURN) handle %u: ret %d\n",
		 mode,arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

status_t run_class_init_tests(crypto_context_t *crypto_ctx)
{
	status_t ret = NO_ERROR;

	/* test with wrong indication of algorithm => must fail with ERR_NOT_SUPPORTED */
	TEST_EXPECT_RET(TEST_class_init_function, ERR_NOT_SUPPORTED, CCC_ENGINE_ANY,
			TE_ALG_SHA256, TE_CLASS_RANDOM);
#if HAVE_CCC_KDF
	TEST_EXPECT_RET(TEST_class_init_function, ERR_NOT_SUPPORTED, CCC_ENGINE_ANY,
			TE_ALG_SHA256, TE_CLASS_KEY_DERIVATION);
#endif /* HAVE_CCC_KDF */
	TEST_EXPECT_RET(TEST_class_init_function, ERR_NOT_SUPPORTED, CCC_ENGINE_ANY,
			TE_ALG_SHA256, TE_CLASS_MAC);
	TEST_EXPECT_RET(TEST_class_init_function, ERR_NOT_SUPPORTED, CCC_ENGINE_ANY,
			TE_ALG_RANDOM_DRNG, TE_CLASS_DIGEST);
	TEST_EXPECT_RET(TEST_class_init_function, ERR_NOT_SUPPORTED, CCC_ENGINE_ANY,
			TE_ALG_RANDOM_DRNG, TE_CLASS_ASYMMETRIC_CIPHER);
	TEST_EXPECT_RET(TEST_class_init_function, ERR_NOT_SUPPORTED, CCC_ENGINE_ANY,
			TE_ALG_RANDOM_DRNG, TE_CLASS_ASYMMETRIC_SIGNATURE);
	TEST_EXPECT_RET(TEST_class_init_function, ERR_ILWALID_ARGS, CCC_ENGINE_ANY,
			TE_ALG_RANDOM_DRNG, TE_CLASS_CIPHER);
#if HAVE_AES_AEAD
	TEST_EXPECT_RET(TEST_class_init_function, ERR_NOT_SUPPORTED, CCC_ENGINE_ANY,
			TE_ALG_RANDOM_DRNG, TE_CLASS_AE);
#endif /* HAVE_AES_AEAD */

	if (CFALSE) {
	fail:
		LOG_ERROR("[ ***ERROR: CLASS INIT TEST CASE FAILED! (err 0x%x) ]\n",ret);
		// On failure must return error code
		if (NO_ERROR == ret) {
			ret = SE_ERROR(ERR_GENERIC);
		}
	}
	return ret;
}
#endif /* KERNEL_TEST_MODE */

