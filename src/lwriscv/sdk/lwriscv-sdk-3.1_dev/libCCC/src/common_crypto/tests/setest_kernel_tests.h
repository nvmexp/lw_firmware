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
#ifndef INCLUDED_SETEST_KERNEL_TESTS_H
#define INCLUDED_SETEST_KERNEL_TESTS_H

#include <include/crypto_api.h>
#include <crypto_ops.h>
#include <tests/setest_kernel_tests_defs.h>
#include <tests/setest_kernel_tests_protos.h>

#undef LOG_INFO
#define LOG_INFO LTRACEF

#if 1

#define memory_compare se_util_mem_cmp

#define memcmp INCORRECT_memcmp
#define memset INCORRECT_memset
#define memmove INCORRECT_memmove
#define memcpy INCORRECT_memcpy

#else

static inline uint32_t memory_compare(const void *a, const void *b, uint32_t len)
{
	status_t ret = NO_ERROR;
	bool res = false;

	(void)ret;

	if (0U == len) {
		/* Zero byte compare on non-NULL vectors is OK */
		res = ((NULL != a) && (NULL != b));
	} else {
		res = se_util_vec_match(a, b, len, false, &ret);
	}

	return (res ? 0U : 1U);
}
#endif

#define op_INIT      (TE_OP_INIT)
#define op_INIT_KEY  (TE_OP_INIT | TE_OP_SET_KEY)
#define op_INIT_DOFINAL (TE_OP_INIT | TE_OP_DOFINAL)
#define op_FINAL_REL (TE_OP_DOFINAL | TE_OP_RESET)

#define RSA512_KEY_BITS 512U
#define RSA512_BYTE_SIZE (RSA512_KEY_BITS / 8U)

#define RSA2048_KEY_BITS 2048U
#define RSA2048_BYTE_SIZE (RSA2048_KEY_BITS / 8U)

#define RSA4096_KEY_BITS 4096U
#define RSA4096_BYTE_SIZE (RSA4096_KEY_BITS / 8U)

#define RSA3072_KEY_BITS 3072U
#define RSA3072_BYTE_SIZE (RSA3072_KEY_BITS / 8U)

#define DH2048_BYTE_SIZE (2048U / 8U)

/* Allow CCC api call macros to be redefined is required
 */
#ifndef CRYPTO_CONTEXT_SETUP
#define CRYPTO_CONTEXT_SETUP(c, mode,algo)				\
	do {								\
		ret = crypto_kernel_context_setup(c, __LINE__, mode, algo); \
		CCC_ERROR_CHECK(ret);				\
	} while(CFALSE);

#define CRYPTO_OPERATION(c, ap) crypto_handle_operation(c, ap)
#define CRYPTO_CONTEXT_RESET(c)				\
	do { if (NULL != (c)) {				\
			crypto_kernel_context_reset(c);	\
	     }						\
	} while (CFALSE)
#endif /* CRYPTO_CONTEXT_SETUP */

#define DUMP_HEX(name,base,len)						\
	do { LOG_INFO("%s =>\n", __func__); LOG_HEXBUF(name, base, len); } while(CFALSE)

#define VERIFY_ARRAY_VALUE(result, expected, len)			\
	do { if (memory_compare(result, expected, len) != 0) {			\
			LOG_ERROR("ERROR: value mismatch (" #result " != " #expected ")[size %u]\n", \
				  len);					\
			DUMP_HEX(#result , result, len);		\
			DUMP_HEX(#expected , expected, len);		\
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);	\
		}							\
		LOG_INFO("[ Result verified (%s:%u) ]\n",		\
			 __func__, __LINE__);				\
	} while(CFALSE)

#define TRAP_ERROR(...)							\
	do { LOG_ERROR("***ERROR in test (%s:%u): ret=%d\n", __func__, __LINE__, ret); \
		LOG_ERROR(__VA_ARGS__);					\
		goto fail;						\
	} while(CFALSE)

#define TRAP_IF_ERROR(ret, ...)						\
	if (NO_ERROR != (ret)) {					\
		LOG_ERROR("***ERROR in test (%s:%u): ret=%d\n", __func__, __LINE__, ret); \
		LOG_ERROR(__VA_ARGS__);					\
		goto fail;						\
	}

#define TRAP_ASSERT(x)							\
	do { if (!BOOL_IS_TRUE(x)) {					\
			ret = SE_ERROR(ERR_GENERIC);			\
			LOG_ERROR("***ERROR in test (%s:%u): setting ret=%d\n", \
				  __func__, __LINE__, ret);		\
			LOG_ERROR("false value in: %s\n", #x);		\
			goto fail;					\
		}							\
	} while(CFALSE)

/* test driver macros */

#define TEST_ERRCHK_FUN(fun, string, ...)				\
	do {    LOG_ALWAYS("\n[ starting test (%u) " string "\n", __LINE__); \
		ret = fun(##__VA_ARGS__);				\
		CCC_ERROR_CHECK(ret,				\
				    LOG_ERROR("[ TEST FAILURE ] (@%u) " #fun " " string " return: %d\n", \
					      __LINE__, ret));		\
		LOG_ALWAYS("[ completed test " #fun " " string " ]\n\n"); \
	} while (CFALSE)


#define TEST_ERRCHK(fun, engine_id, cmode, ...)				\
	do {    LOG_ALWAYS("\n[ starting test " #fun " for " #cmode " (@%u) (engine: " #engine_id ")\n", __LINE__); \
		ret = fun(crypto_ctx, cmode, engine_id, ##__VA_ARGS__);	\
		CCC_ERROR_CHECK(ret,				\
				    LOG_ERROR("[ TEST FAILURE ] (@%u) " #fun " " #cmode " return: %d\n", \
					      __LINE__, ret));		\
		LOG_ALWAYS("[ completed test " #fun " for " #cmode " ]\n\n"); \
	} while (CFALSE)

#define TEST_EXPECT_RET(fun, ecode, engine_id, cmode, ...)		\
	do {    LOG_ALWAYS("\n[ starting test " #fun " for " #cmode " (@%u) (engine: " #engine_id ")\n", __LINE__); \
		ret = fun(crypto_ctx, cmode, engine_id, ##__VA_ARGS__);	\
		if (ecode != ret) {					\
			LOG_ERROR("[ TEST FAILURE ] (@%u) ECODE mismatch (expect 0x%x)" #fun " " #cmode " return: 0x%x (%d)\n", \
				  __LINE__, ecode, ret,ret);		\
			goto fail;					\
		}							\
		LOG_ALWAYS("[ expected fail code %d completed test " #fun " for " #cmode " ]\n\n", ecode); \
		ret = NO_ERROR;						\
	} while (CFALSE)

#define TEST_SUITE(name, fun)						\
	do { LOG_ALWAYS("\n[ TEST SUITE(%s) line %u START ]\n", name, __LINE__); \
		ret = fun;						\
		CCC_ERROR_CHECK(ret,				\
				    LOG_ERROR("[ TEST SUITE(%s) FAILURE %d ]\n\n", name, ret)); \
		LOG_ALWAYS("[ TEST SUITE(%s) PASSED ]\n\n", name);	\
	} while (CFALSE)

status_t reverse_buffer(unsigned char *buf, uint32_t blen);
const char *eid_name(engine_id_t eid);

#endif /* INCLUDED_SETEST_KERNEL_TESTS_H */
