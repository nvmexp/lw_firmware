/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION. All rights reserved
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
#ifndef INCLUDED_SETEST_KERNEL_TESTS_PROTOS_H
#define INCLUDED_SETEST_KERNEL_TESTS_PROTOS_H

#ifdef TEST_PSC
status_t run_psc_tests(crypto_context_t *crypto_ctx, engine_id_t engine_id);
#endif

status_t run_NIST_aes_tests(crypto_context_t *crypto_ctx, engine_id_t engine_id);
status_t run_NIST_async_aes_tests(crypto_context_t *crypto_ctx, engine_id_t engine_id);
status_t run_aes_xts_tests(crypto_context_t *crypto_ctx, engine_id_t engine_id);
status_t run_rsa_cipher_tests(crypto_context_t *crypto_ctx, engine_id_t engine_id);
status_t run_wyche_ecdsa_tests(crypto_context_t *crypto_ctx, engine_id_t engine_id);
status_t run_wyche_ecdh_tests(crypto_context_t *crypto_ctx, engine_id_t engine_id);
#if HAVE_AES_CCM
status_t run_aes_ccm_tests(crypto_context_t *crypto_ctx, engine_id_t engine_id);
#endif /* HAVE_AES_CCM */

#if HAVE_AES_GCM
status_t run_aes_gcm_tests(crypto_context_t *crypto_ctx, engine_id_t engine_id);
#endif

#if HAVE_ENGINE_API_TESTS
status_t run_engine_api_tests(void);
#endif
status_t run_rsa_sign_tests(crypto_context_t *crypto_ctx, engine_id_t engine_id);

#ifdef TEST_RSASP1
status_t run_rsasp1_tests(crypto_context_t *crypto_ctx, engine_id_t engine_id);
#endif

#ifdef TEST_RECIPROCAL
status_t run_reciprocal_hash_test(crypto_context_t *crypto_ctx, engine_id_t engine_id);
#endif

#if HAVE_CMAC_AES
status_t run_mac_test_cases(crypto_context_t *crypto_ctx);
#if HAVE_CMAC_DST_KEYTABLE
status_t run_cmac_to_keytable_test_cases(crypto_context_t *crypto_ctx);
#endif /* HAVE_CMAC_DST_KEYTABLE */
#endif /* HAVE_CMAC_AES */

#if HAVE_HMAC_SHA
status_t TEST_hmac(crypto_context_t *c, te_crypto_algo_t mac, engine_id_t eid);
status_t run_hmac_test_cases(crypto_context_t *crypto_ctx);
#endif

#if HAVE_HMAC_SHA || HAVE_CMAC_AES
status_t TEST_mac_empty(crypto_context_t *c, te_crypto_algo_t algo, engine_id_t eid);
#endif

#if HAVE_SE_AES
status_t run_cipher_test_cases(crypto_context_t *crypto_ctx);
#endif /* HAVE_SE_AES */

#ifdef TEST_ED25519_SIGN
status_t run_ed25519_sign_verify_test_cases(crypto_context_t *crypto_ctx);
#endif

#ifdef TEST_ED25519
status_t run_ed25519_test_cases(crypto_context_t *crypto_ctx);
#endif

status_t run_class_init_tests(crypto_context_t *crypto_ctx);

#ifdef TEST_SEC_SAFE_REQ_VERIFY
status_t run_ccc_req_verify_tests(crypto_context_t *crypto_ctx);
#endif

#ifdef TEST_CCC_PLC_API_TESTS
status_t run_ccc_api_tests(crypto_context_t *crypto_ctx);
#endif

#ifdef TEST_AES_KDF_TESTS
status_t run_aes_kdf_test_cases(crypto_context_t *crypto_ctx);
#endif

#ifdef TEST_CMAC_KDF_TESTS
status_t run_cmac_kdf_test_cases(crypto_context_t *crypto_ctx);
#endif

#ifdef TEST_ECDSA
status_t run_ecdsa_test_cases(crypto_context_t *crypto_ctx);
#endif

#ifdef TEST_ECDSA_SIGN
status_t run_ecdsa_sign_test_cases(crypto_context_t *crypto_ctx);
#endif

/* In SRS tests */
#if HAVE_CMAC_AES
status_t tegra_se_aes_cmac_hw_reg_clear_verify(engine_id_t eid);
#endif

/* In SRS tests */
#ifdef TEST_DIGEST
status_t tegra_se_sha_hw_reg_clear_verify(uint32_t result_size);
#endif

#ifdef TEST_XMSS
status_t run_xmss_test_cases(crypto_context_t *crypto_ctx, engine_id_t engine_id);
#endif

#endif /* INCLUDED_SETEST_KERNEL_TESTS_PROTOS_H */
