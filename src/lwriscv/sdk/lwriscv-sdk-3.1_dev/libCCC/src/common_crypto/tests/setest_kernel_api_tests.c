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

#ifdef TEST_CCC_PLC_API_TESTS

__STATIC__ status_t TEST_ccc_safe_arith_negative_test(crypto_context_t *c,
		te_crypto_algo_t algo, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	uint32_t res = 0U;

	(void)c;
	(void)algo;
	(void)eid;

	LOG_ALWAYS("T19X code coverage test case for se_util_log_error_string\n");

	CCC_SAFE_DIV_U32(res, 1U, 0U);
	CCC_ERROR_CHECK(ret);

	//CCC_SAFE_ADD_U32(res, CCC_UINT_MAX, 1U);
	//CCC_ERROR_CHECK(ret);

fail:
	return ret;
}

status_t run_ccc_api_tests(crypto_context_t *crypto_ctx)
{
	status_t ret = NO_ERROR;

	/* Negative test for CCC_SAFE_XXX calls */
	TEST_EXPECT_RET(TEST_ccc_safe_arith_negative_test, ERR_NOT_VALID,
			CCC_ENGINE_SE0_AES0, TE_ALG_AES_CTR);
fail:
	return ret;
}
#endif /* TEST_CCC_PLC_API_TESTS */
#endif /* KERNEL_TEST_MODE */
