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

/* The data vectors are from NIST publication 800-38.a
 * All results are verified for the exelwted operations.
 *
 * The CFB*() operations are not exelwted since the SE/HW does not
 * support AES-CFB mode.
 */

#include <crypto_system_config.h>
#include <tests/setest_kernel_tests.h>

#if KERNEL_TEST_MODE

#include <crypto_api.h>
#include <crypto_ops.h>

#if HAVE_AES_GCM

#define AES_KEYSLOT 2U

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#define AES_GCM_IV_SIZE 12U

status_t run_aes_gcm_tests(crypto_context_t *crypto_ctx, engine_id_t engine_id);

static status_t aes_gcm_test_cipher(crypto_context_t *c,
				    const char *test_name,
				    uint32_t count,
				    engine_id_t engine_id,
				    te_crypto_alg_mode_t mode,
				    uint32_t keybytes, const uint8_t *key,
				    const uint8_t *input64, const uint8_t *output64,
				    const uint8_t *nonce, uint32_t num_bytes,
				    const uint8_t *aad, uint32_t aad_len,
				    const uint8_t *tag, uint32_t tag_len)
{
	status_t ret = NO_ERROR;
	te_crypto_algo_t algo = TE_ALG_AES_GCM;
	te_crypto_args_t arg = { .ca_handle = 0U, };
	DMA_ALIGN_DATA uint8_t result64[128U];

	/* Making static is not good, but keeps the object out of stack
	 * in this TEST CODE run!!!
	 */
	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
	};

	if (NULL == test_name) {
		test_name = "";
	}

	if (NULL == aad) {
		aad_len = 0U;
	}

	if ((NULL == c) ||
	    (NULL == key) || (NULL == nonce) || (NULL == tag)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == input64) {
		num_bytes = 0U;
	}

	if (NULL == output64) {
		if (NULL != input64) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}
	}

	if (num_bytes > sizeof_u32(result64)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LOG_ERROR("Input data too long for result buffer\n"));
	}

	LOG_INFO("\n");
	LOG_INFO("[ AES-GCM vector test: %s # %u START ]\n", test_name, count);

	ret = crypto_kernel_context_setup(c, 0x42U, mode, algo);
	CCC_ERROR_CHECK(ret);

	se_util_mem_set(result64, 0U, sizeof_u32(result64));

	LTRACEF("SRC num_bytes: %u\n", num_bytes);

	arg.ca_algo     = algo;
	arg.ca_alg_mode = mode;

	// Request to use the crypto engine given as argument
	// SE driver may use it or ignore this.
	//
	arg.ca_init.engine_hint = engine_id;

	// arg.ca_init.aes.gcm_mode.nonce_len = AES_GCM_NONCE_LENGTH;
	se_util_mem_move(arg.ca_init.aes.gcm_mode.nonce, nonce, AES_GCM_IV_SIZE);

	akey.k_byte_size = keybytes;
	se_util_mem_move(akey.k_aes_key, key, keybytes);

	/* Move the caller TAG value so that it can be verified by the
	 * decipher call. If the tag deos not match the decipher call retuns
	 * error ERR_NOT_VALID
	 */
	if (TE_ALG_MODE_DECRYPT == mode) {
		se_util_mem_move(&arg.ca_init.aes.gcm_mode.tag[0],
				 tag, tag_len);
	}

	// GCM operates on this many byte tag (default 16)
	arg.ca_init.aes.gcm_mode.tag_len = tag_len;

	arg.ca_set_key.kdata = &akey;

	if ((aad != NULL) && (0U != aad_len)) {
		arg.ca_opcode   = TE_OP_INIT | TE_OP_SET_KEY | TE_OP_UPDATE;

		/* AAD input is identified by setting DST NULL and DST_SIZE 0U
		 * Any number of AAD update calls are allowed; on first call
		 * with DST being nonzero => AAD input mode is terminated.
		 */
		arg.ca_data.src_size = aad_len;
		arg.ca_data.src = aad;
		arg.ca_data.dst_size = 0U;
		arg.ca_data.dst = NULL;

		LOG_ERROR("AES-GCM => AAD input: %u bytes\n", aad_len);

		ret = crypto_handle_operation(c, &arg);
		CCC_ERROR_CHECK(ret,
				LOG_ERROR("%s: CRYPTO(%s): syscall (%d); "
					  "ALGO: 0x%x, key: %u, MODE: %u\n",
					  test_name, __func__, ret,
					  algo, (keybytes * 8U), mode));

		/* Then complete AES-GCM setting DST not null and pass in
		 * the data to cipher with GCM. This will indicate to CCC
		 * that the AAD input is complete and all update/dofinal calls
		 * after this must have DST and DST_LEN properly set (or
		 * error is trapped). This is because the encryption produces
		 * output to DST but Tag callwlation does not.
		 */
		arg.ca_opcode   = TE_OP_DOFINAL | TE_OP_RESET;

		LOG_ERROR("AES-GCM => CIPHER input: %u bytes\n", num_bytes);

		arg.ca_data.src_size = num_bytes;
		arg.ca_data.src = input64;
		arg.ca_data.dst_size = num_bytes;
		arg.ca_data.dst = result64;

		ret = crypto_handle_operation(c, &arg);
		CCC_ERROR_CHECK(ret,
				LOG_ERROR("%s: CRYPTO(%s): syscall (%d); "
					  "ALGO: 0x%x, key: %u, MODE: %u\n",
					  test_name, __func__, ret,
					  algo, (keybytes * 8U), mode));
	} else {
		arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

		arg.ca_data.src_size = num_bytes;
		arg.ca_data.src = input64;
		arg.ca_data.dst_size = num_bytes;
		arg.ca_data.dst = result64;

		LOG_ERROR("AES-GCM => CIPHER input: %u bytes\n", num_bytes);

		ret = crypto_handle_operation(c, &arg);
		CCC_ERROR_CHECK(ret,
				LOG_ERROR("%s: CRYPTO(%s): syscall (%d); "
					  "ALGO: 0x%x, key: %u, MODE: %u\n",
					  test_name, __func__, ret,
					  algo, (keybytes * 8U), mode));
	}

	LOG_HEXBUF(test_name, result64, arg.ca_data.dst_size);

	LTRACEF("GCM tag: bytes %u\n", tag_len);
	LOG_HEXBUF("AUTH TAG (callwlated)",
		   &arg.ca_init.aes.gcm_mode.tag[0], tag_len);

	if (0U != memory_compare(result64, output64, num_bytes)) {
		LOG_HEXBUF("incorrect", result64, arg.ca_data.dst_size);
		LOG_HEXBUF("Correct", output64, num_bytes);
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LOG_ERROR("%s: CRYPTO(%s): result compare failed; "
					       "ALGO: 0x%x, key: %u, MODE: %u\n",
					       test_name, __func__, algo, (keybytes * 8U), mode));

	}

	LTRACEF("GCM comparing tag\n");

	if (0U != memory_compare(tag, &arg.ca_init.aes.gcm_mode.tag[0], tag_len)) {
		LOG_HEXBUF("Callwlated TAG (wrong)", &arg.ca_init.aes.gcm_mode.tag[0], tag_len);
		LOG_HEXBUF("Correct TAG (OK)", tag, tag_len);
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LOG_ERROR("%s: CRYPTO(%s): AUTH TAG compare failed; "
					       "ALGO: 0x%x, key: %u, MODE: %u\n",
					       test_name, __func__, algo, (keybytes * 8U), mode));
	}

	LOG_INFO("[ AES-GCM vector test: %s(%u) PASSED ]\n", test_name, count);
fail:
	if (NO_ERROR != ret) {
		LOG_ERROR("[ AES-GCM vector test: %s(%u) FAILED: %d]\n",
			  test_name, count, ret);
	}

	if (NULL != c) {
		crypto_kernel_context_reset(c);
	}
	return ret;
}

#define AES_GCM_TEST(nname, count, cmode, input, output,		\
		     num_bytes, aad, aadlen, tag, taglen)		\
	do { ret = aes_gcm_test_cipher(crypto_ctx, nname, count, engine_id, \
				       TE_ALG_MODE_ ## cmode,		\
				       sizeof_u32(Key), Key,		\
				       input, output, IV, num_bytes,	\
				       aad, aadlen,			\
				       tag, taglen);			\
	     CCC_ERROR_CHECK(ret,					\
			     LOG_ERROR("[ AES-GCM vector "		\
				       "test %s (line %u) FAILED (%d)]\n", \
				       (nname?nname:""), __LINE__, ret)); \
	} while(0)

static status_t run_aes_128_gcm_tests(crypto_context_t *crypto_ctx,
				      engine_id_t engine_id)
{
	status_t ret = NO_ERROR;
	uint32_t COUNT = 0U;
	uint32_t nbytes = 0U;
	uint32_t AADLEN = 0U;
	uint32_t Taglen = 16U;

	LOG_INFO("[AES-128-GCM cipher test starting]\n");

	{
		COUNT = 0;
		nbytes = 16;

		static const uint8_t Key[] =
			{ 0x7f, 0xdd, 0xb5, 0x74, 0x53, 0xc2, 0x41, 0xd0,
			  0x3e, 0xfb, 0xed, 0x3a, 0xc4, 0x4e, 0x37, 0x1c,
			};

		static const uint8_t IV[] =
			{ 0xee, 0x28, 0x3a, 0x3f, 0xc7, 0x55, 0x75, 0xe3,
			  0x3e, 0xfd, 0x48, 0x87,
			};

		static const uint8_t PT[] =
			{ 0xd5, 0xde, 0x42, 0xb4, 0x61, 0x64, 0x6c, 0x25,
			  0x5c, 0x87, 0xbd, 0x29, 0x62, 0xd3, 0xb9, 0xa2,
			};

		static const uint8_t CT[] =
			{ 0x2c, 0xcd, 0xa4, 0xa5, 0x41, 0x5c, 0xb9, 0x1e,
			  0x13, 0x5c, 0x2a, 0x0f, 0x78, 0xc9, 0xb2, 0xfd,
			};

		static const uint8_t Tag[] =
			{ 0xb3, 0x6d, 0x1d, 0xf9, 0xb9, 0xd5, 0xe5, 0x96,
			  0xf8, 0x3e, 0x8b, 0x7f, 0x52, 0x97, 0x1c, 0xb3,
			};

		uint8_t *AAD = NULL;
		AADLEN = 0U;

		AES_GCM_TEST("ENCRYPT", COUNT, ENCRYPT, PT, CT, nbytes,
			     AAD, AADLEN, Tag, Taglen);

		AES_GCM_TEST("DECRYPT", COUNT, DECRYPT, CT, PT, nbytes,
			     AAD, AADLEN, Tag, Taglen);
	}

	{
		COUNT = 1;
		nbytes = 16;

		static const uint8_t Key[16] = { 0x00, };
		static const uint8_t IV[12]  = { 0x00, };
		static const uint8_t PT[16]  = { 0x00, };
		static const uint8_t CT[]    =
			{ 0x03, 0x88, 0xda, 0xce, 0x60, 0xb6, 0xa3, 0x92,
			  0xf3, 0x28, 0xc2, 0xb9, 0x71, 0xb2, 0xfe, 0x78, };
		static const uint8_t *AAD = NULL;
		static const uint8_t Tag[] =
			{ 0xab, 0x6e, 0x47, 0xd4, 0x2c, 0xec, 0x13, 0xbd,
			  0xf5, 0x3a, 0x67, 0xb2, 0x12, 0x57, 0xbd, 0xdf, };

		AADLEN = 0U;

		AES_GCM_TEST("ENCRYPT", COUNT, ENCRYPT, PT, CT, nbytes,
			     AAD, AADLEN, Tag, Taglen);

		AES_GCM_TEST("DECRYPT", COUNT, DECRYPT, CT, PT, nbytes,
			     AAD, AADLEN, Tag, Taglen);
	}

	{
		COUNT = 2;
		static const uint8_t Key[] =
			{ 0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c,
			  0x6d, 0x6a, 0x8f, 0x94, 0x67, 0x30, 0x83, 0x08, };
		static const uint8_t IV[12] =
			{ 0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce, 0xdb, 0xad,
			  0xde, 0xca, 0xf8, 0x88, };
		static const uint8_t CT[]   =
			{ 0x42, 0x83, 0x1e, 0xc2, 0x21, 0x77, 0x74, 0x24,
			  0x4b, 0x72, 0x21, 0xb7, 0x84, 0xd0, 0xd4, 0x9c,
			  0xe3, 0xaa, 0x21, 0x2f, 0x2c, 0x02, 0xa4, 0xe0,
			  0x35, 0xc1, 0x7e, 0x23, 0x29, 0xac, 0xa1, 0x2e,
			  0x21, 0xd5, 0x14, 0xb2, 0x54, 0x66, 0x93, 0x1c,
			  0x7d, 0x8f, 0x6a, 0x5a, 0xac, 0x84, 0xaa, 0x05,
			  0x1b, 0xa3, 0x0b, 0x39, 0x6a, 0x0a, 0xac, 0x97,
			  0x3d, 0x58, 0xe0, 0x91, 0x47, 0x3f, 0x59, 0x85, };
		static const uint8_t *AAD = NULL;
		static const uint8_t Tag[] =
			{ 0x4d, 0x5c, 0x2a, 0xf3, 0x27, 0xcd, 0x64, 0xa6,
			  0x2c, 0xf3, 0x5a, 0xbd, 0x2b, 0xa6, 0xfa, 0xb4, };
		static const uint8_t PT[] = { 0xd9, 0x31, 0x32, 0x25, 0xf8, 0x84, 0x06, 0xe5,
				 0xa5, 0x59, 0x09, 0xc5, 0xaf, 0xf5, 0x26, 0x9a,
				 0x86, 0xa7, 0xa9, 0x53, 0x15, 0x34, 0xf7, 0xda,
				 0x2e, 0x4c, 0x30, 0x3d, 0x8a, 0x31, 0x8a, 0x72,
				 0x1c, 0x3c, 0x0c, 0x95, 0x95, 0x68, 0x09, 0x53,
				 0x2f, 0xcf, 0x0e, 0x24, 0x49, 0xa6, 0xb5, 0x25,
				 0xb1, 0x6a, 0xed, 0xf5, 0xaa, 0x0d, 0xe6, 0x57,
				 0xba, 0x63, 0x7b, 0x39, 0x1a, 0xaf, 0xd2, 0x55,
		};
		nbytes = sizeof_u32(PT);
		AADLEN = 0U;

		AES_GCM_TEST("ENCRYPT", COUNT, ENCRYPT, PT, CT, nbytes,
			     AAD, AADLEN, Tag, Taglen);

		AES_GCM_TEST("DECRYPT", COUNT, DECRYPT, CT, PT, nbytes,
			     AAD, AADLEN, Tag, Taglen);
	}

	{
		COUNT = 3;
		static const uint8_t Key[] =
			{ 0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c,
			  0x6d, 0x6a, 0x8f, 0x94, 0x67, 0x30, 0x83, 0x08, };
		static const uint8_t IV[] =
			{ 0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce, 0xdb, 0xad,
			  0xde, 0xca, 0xf8, 0x88, };
		static const uint8_t CT[] = { 0x42, 0x83, 0x1e, 0xc2, 0x21, 0x77, 0x74, 0x24,
				 0x4b, 0x72, 0x21, 0xb7, 0x84, 0xd0, 0xd4, 0x9c,
				 0xe3, 0xaa, 0x21, 0x2f, 0x2c, 0x02, 0xa4, 0xe0,
				 0x35, 0xc1, 0x7e, 0x23, 0x29, 0xac, 0xa1, 0x2e,
				 0x21, 0xd5, 0x14, 0xb2, 0x54, 0x66, 0x93, 0x1c,
				 0x7d, 0x8f, 0x6a, 0x5a, 0xac, 0x84, 0xaa, 0x05,
				 0x1b, 0xa3, 0x0b, 0x39, 0x6a, 0x0a, 0xac, 0x97,
				 0x3d, 0x58, 0xe0, 0x91, };
		static const uint8_t AAD[] =
			{ 0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef,
			  0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef,
			  0xab, 0xad, 0xda, 0xd2, };
		static const uint8_t Tag[] =
			{ 0x5b, 0xc9, 0x4f, 0xbc, 0x32, 0x21, 0xa5, 0xdb,
			  0x94, 0xfa, 0xe9, 0x5a, 0xe7, 0x12, 0x1a, 0x47, };
		static const uint8_t PT[]  =
			{ 0xd9, 0x31, 0x32, 0x25, 0xf8, 0x84, 0x06, 0xe5,
			  0xa5, 0x59, 0x09, 0xc5, 0xaf, 0xf5, 0x26, 0x9a,
			  0x86, 0xa7, 0xa9, 0x53, 0x15, 0x34, 0xf7, 0xda,
			  0x2e, 0x4c, 0x30, 0x3d, 0x8a, 0x31, 0x8a, 0x72,
			  0x1c, 0x3c, 0x0c, 0x95, 0x95, 0x68, 0x09, 0x53,
			  0x2f, 0xcf, 0x0e, 0x24, 0x49, 0xa6, 0xb5, 0x25,
			  0xb1, 0x6a, 0xed, 0xf5, 0xaa, 0x0d, 0xe6, 0x57,
			  0xba, 0x63, 0x7b, 0x39, };

		nbytes = sizeof_u32(PT);
		AADLEN = sizeof_u32(AAD);

		AES_GCM_TEST("ENCRYPT", COUNT, ENCRYPT, PT, CT, nbytes,
			     AAD, AADLEN, Tag, Taglen);

		AES_GCM_TEST("DECRYPT", COUNT, DECRYPT, CT, PT, nbytes,
			     AAD, AADLEN, Tag, Taglen);
	}

	{
		// web page www.ieee802.org/1/files/public/docs2011/bn-randall-test-vectors-0511-v1.pdf
		// section 2.2.1 first test (60 byte packet encryption GCM-AES-128)
		COUNT = 221;

		static const uint8_t Key[] =
			{ 0xAD, 0x7A, 0x2B, 0xD0, 0x3E, 0xAC, 0x83, 0x5A, 0x6F, 0x62, 0x0F, 0xDC, 0xB5, 0x06, 0xB3, 0x45,
			};
		static const uint8_t IV[] =
			{ 0x12, 0x15, 0x35, 0x24, 0xC0, 0x89, 0x5E, 0x81, 0xB2, 0xC2, 0x84, 0x65, };
		static const uint8_t CT[] = {
			0x70, 0x1A, 0xFA, 0x1C, 0xC0, 0x39, 0xC0, 0xD7, 0x65, 0x12, 0x8A, 0x66, 0x5D, 0xAB, 0x69, 0x24,
			0x38, 0x99, 0xBF, 0x73, 0x18, 0xCC, 0xDC, 0x81, 0xC9, 0x93, 0x1D, 0xA1, 0x7F, 0xBE, 0x8E, 0xDD,
			0x7D, 0x17, 0xCB, 0x8B, 0x4C, 0x26, 0xFC, 0x81, 0xE3, 0x28, 0x4F, 0x2B, 0x7F, 0xBA, 0x71, 0x3D,
		};

		static const uint8_t AAD[] =
			{ 0xD6, 0x09, 0xB1, 0xF0, 0x56, 0x63, 0x7A, 0x0D, 0x46, 0xDF, 0x99, 0x8D, 0x88, 0xE5, 0x2E, 0x00,
			  0xB2, 0xC2, 0x84, 0x65, 0x12, 0x15, 0x35, 0x24, 0xC0, 0x89, 0x5E, 0x81,
			};

		static const uint8_t Tag[] =
			{
				0x4F, 0x8D, 0x55, 0xE7, 0xD3, 0xF0, 0x6F, 0xD5, 0xA1, 0x3C, 0x0C, 0x29, 0xB9, 0xD5, 0xB8, 0x80,
			};
		static const uint8_t PT[]  =
			{
				0x08, 0x00, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
				0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C,
				0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x00, 0x02,
			};

		nbytes = sizeof_u32(PT);
		AADLEN = sizeof_u32(AAD);

		AES_GCM_TEST("IEEE/ENCRYPT", COUNT, ENCRYPT, PT, CT, nbytes,
			     AAD, AADLEN, Tag, Taglen);

		AES_GCM_TEST("IEEE/DECRYPT", COUNT, DECRYPT, CT, PT, nbytes,
			     AAD, AADLEN, Tag, Taglen);
	}

	{
		/* This is identical to GMAC: No plaintext or ciphertext provided */
		COUNT = 4;

		static const uint8_t Key[] =
			{ 0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c,
			  0x6d, 0x6a, 0x8f, 0x94, 0x67, 0x30, 0x83, 0x08, };
		static const uint8_t IV[] =
			{ 0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce, 0xdb, 0xad,
			  0xde, 0xca, 0xf8, 0x88, };
		static const uint8_t *CT = NULL;

		static const uint8_t AAD[] =
			{ 0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef,
			  0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef,
			  0xab, 0xad, 0xda, 0xd2, };

		static const uint8_t Tag[] =
			{
				0x34, 0x64, 0x34, 0xFD, 0x51, 0xD5, 0xCD, 0x0C, 0x58, 0x87, 0xEC, 0x63, 0xE3, 0x9B, 0x90, 0x7A,
			};

		static const uint8_t *PT = NULL;

		nbytes = 0U;
		AADLEN = sizeof_u32(AAD);

		AES_GCM_TEST("ENCRYPT", COUNT, ENCRYPT, PT, CT, nbytes,
			     AAD, AADLEN, Tag, Taglen);

		AES_GCM_TEST("DECRYPT", COUNT, DECRYPT, CT, PT, nbytes,
			     AAD, AADLEN, Tag, Taglen);
	}

	LOG_INFO("[AES-128-GCM cipher tests: PASSED]\n");
fail:
	return ret;
}

static status_t gmac_aes_test(crypto_context_t *c,
			      const char *test_name,
			      uint32_t count,
			      engine_id_t engine_id,
			      te_crypto_alg_mode_t mode,
			      uint32_t keybytes, const uint8_t *key,
			      const uint8_t *nonce,
			      const uint8_t *aad, uint32_t aad_len,
			      const uint8_t *tag, uint32_t tag_len)
{
	status_t ret = NO_ERROR;
	te_crypto_algo_t algo = TE_ALG_GMAC_AES;
	te_crypto_args_t arg = { .ca_handle = 0U, };
	uint8_t gmac[128U];

	/* Making static is not good, but keeps the object out of stack
	 * in this TEST CODE run!!!
	 */
	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
	};

	(void)tag_len;

	if (NULL == test_name) {
		test_name = "";
	}

	if (NULL == aad) {
		aad_len = 0U;
	}

	if ((NULL == c) ||
	    (NULL == key) || (NULL == nonce) || (NULL == tag)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	LOG_INFO("\n");
	LOG_INFO("[ GMAC-AES vector test: %s # %u START ]\n", test_name, count);

	ret = crypto_kernel_context_setup(c, 0x42U, mode, algo);
	CCC_ERROR_CHECK(ret);

	se_util_mem_set(gmac, 0U, sizeof_u32(gmac));

	arg.ca_algo     = algo;
	arg.ca_alg_mode = mode;

	// Request to use the crypto engine given as argument
	// SE driver may use it or ignore this.
	//
	arg.ca_init.engine_hint = engine_id;

	arg.ca_init.aes.gcm_mode.nonce_len = AES_GCM_NONCE_LENGTH;
	se_util_mem_move(&arg.ca_init.aes.gcm_mode.nonce[0], nonce, AES_GCM_NONCE_LENGTH);

	// GMAC returns this many bytes in tag (default 16)
	arg.ca_init.aes.gcm_mode.tag_len = tag_len;

	akey.k_byte_size = keybytes;
	se_util_mem_move(akey.k_aes_key, key, keybytes);

	arg.ca_set_key.kdata = &akey;

	LTRACEF("AES-GCM => AAD input: %u bytes\n", aad_len);

	if ((aad != NULL) && (0U != aad_len)) {
		arg.ca_opcode   = TE_OP_INIT | TE_OP_SET_KEY | TE_OP_UPDATE;

		/* AAD input is identified by setting DST NULL and DST_SIZE 0U
		 * Any number of AAD update calls are allowed; on first call
		 * with DST being nonzero => AAD input mode is terminated.
		 */
		arg.ca_data.src_size = aad_len;
		arg.ca_data.src = aad;
		arg.ca_data.dst_size = 0U;
		arg.ca_data.dst = NULL;

		LOG_ERROR("GMAC-AES => AAD input: %u bytes\n", aad_len);

		ret = crypto_handle_operation(c, &arg);
		CCC_ERROR_CHECK(ret,
				LOG_ERROR("%s: CRYPTO(%s): syscall (%d); "
					  "ALGO: 0x%x, key: %u, MODE: %u\n",
					  test_name, __func__, ret,
					  algo, (keybytes * 8U), mode));

		// No input to, AAD passed above
		arg.ca_opcode   = TE_OP_DOFINAL | TE_OP_RESET;

		arg.ca_data.src_size = 0U;
		arg.ca_data.src = NULL;
		arg.ca_data.dst_size = sizeof_u32(gmac);
		arg.ca_data.dst = gmac;

		ret = crypto_handle_operation(c, &arg);
		CCC_ERROR_CHECK(ret,
				LOG_ERROR("%s: CRYPTO(%s): syscall (%d); "
					  "ALGO: 0x%x, key: %u, MODE: %u\n",
					  test_name, __func__, ret,
					  algo, (keybytes * 8U), mode));
	} else {
		/* No input to GMAC */
		arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

		arg.ca_data.src_size = 0;
		arg.ca_data.src = NULL;
		arg.ca_data.dst_size = sizeof_u32(gmac);
		arg.ca_data.dst = gmac;

		ret = crypto_handle_operation(c, &arg);
		CCC_ERROR_CHECK(ret,
				LOG_ERROR("%s: CRYPTO(%s): syscall (%d); "
					  "ALGO: 0x%x, key: %u, MODE: %u\n",
					  test_name, __func__, ret,
					  algo, (keybytes * 8U), mode));
	}

	LOG_HEXBUF(test_name, gmac, arg.ca_data.dst_size);

	LTRACEF("GMAC tag: bytes %u\n", tag_len);
	LOG_HEXBUF("AUTH TAG (callwlated)",
		   arg.ca_init.aes.gcm_mode.tag, tag_len);

	LTRACEF("GMAC comparing tag\n");

	if (0U != memory_compare(tag, (uint8_t *)&arg.ca_init.aes.gcm_mode.tag[0], tag_len)) {
		LOG_HEXBUF("Callwlated TAG (wrong)",
			   arg.ca_init.aes.gcm_mode.tag, tag_len);
		LOG_HEXBUF("Correct TAG (OK)", tag, tag_len);
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LOG_ERROR("%s: CRYPTO(%s): AUTH TAG compare failed; "
					       "ALGO: 0x%x, key: %u, MODE: %u\n",
					       test_name, __func__, algo, (keybytes * 8U), mode));
	}

	LOG_INFO("[ GMAC-AES vector test: %s PASSED ]\n", test_name);
fail:
	if (NO_ERROR != ret) {
		LOG_ERROR("[ GMAC-AES vector "
			  "test: %s FAILED: %d]\n", test_name, ret);
	}

	if (NULL != c) {
		crypto_kernel_context_reset(c);
	}
	return ret;
}

#define GMAC_AES_TEST(nname, count, cmode, aad, aadlen, tag, taglen)	\
	do { ret = gmac_aes_test(crypto_ctx, nname, count, engine_id,	\
				 TE_ALG_MODE_ ## cmode,			\
				 sizeof_u32(Key), Key,			\
				 IV,					\
				 aad, aadlen,				\
				 tag, taglen);				\
	     CCC_ERROR_CHECK(ret,					\
			     LOG_ERROR("[ GMAC-AES vector "		\
				       "test %s (line %u) FAILED (%d)]\n", \
				       (nname?nname:""), __LINE__, ret)); \
	} while(0)

static status_t run_gmac_aes_128_tests(crypto_context_t *crypto_ctx,
				       engine_id_t engine_id)
{
	status_t ret = NO_ERROR;
	uint32_t COUNT = 0U;
	uint32_t AADLEN = 0U;
	uint32_t Taglen = 16U;

	LOG_INFO("[ GMAC-AES vector tests starting ]\n");

	{
		COUNT = 3;

		static const uint8_t Key[] =
			{ 0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c,
			  0x6d, 0x6a, 0x8f, 0x94, 0x67, 0x30, 0x83, 0x08, };
		static const uint8_t IV[] =
			{ 0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce, 0xdb, 0xad,
			  0xde, 0xca, 0xf8, 0x88, };
		static const uint8_t AAD[] =
			{ 0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef,
			  0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef,
			  0xab, 0xad, 0xda, 0xd2, };
		static const uint8_t Tag[] =
			{
				0x34, 0x64, 0x34, 0xFD, 0x51, 0xD5, 0xCD, 0x0C, 0x58, 0x87, 0xEC, 0x63, 0xE3, 0x9B, 0x90, 0x7A,
			};

		AADLEN = sizeof_u32(AAD);

		GMAC_AES_TEST("GMAC", COUNT, MAC, AAD, AADLEN, Tag, Taglen);
	}


	{
		// web page www.ieee802.org/1/files/public/docs2011/bn-randall-test-vectors-0511-v1.pdf
		// section 2.1.1 first test (54 byte packet auth GCM-AES-128 => GMAC-AES-128)
		COUNT = 211;

		static const uint8_t Key[] =
			{ 0xAD, 0x7A, 0x2B, 0xD0, 0x3E, 0xAC, 0x83, 0x5A, 0x6F, 0x62, 0x0F, 0xDC, 0xB5, 0x06, 0xB3, 0x45, };
		static const uint8_t IV[] =
			{ 0x12, 0x15, 0x35, 0x24, 0xC0, 0x89, 0x5E, 0x81, 0xB2, 0xC2, 0x84, 0x65,  };
		static const uint8_t AAD[] =
			{ 0xD6, 0x09, 0xB1, 0xF0, 0x56, 0x63, 0x7A, 0x0D, 0x46, 0xDF, 0x99, 0x8D, 0x88, 0xE5, 0x22, 0x2A,
			  0xB2, 0xC2, 0x84, 0x65, 0x12, 0x15, 0x35, 0x24, 0xC0, 0x89, 0x5E, 0x81, 0x08, 0x00, 0x0F, 0x10,
			  0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
			  0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
			  0x31, 0x32, 0x33, 0x34, 0x00, 0x01,
			};

		static const uint8_t Tag[] =
			{ 0xF0, 0x94, 0x78, 0xA9, 0xB0, 0x90, 0x07, 0xD0, 0x6F, 0x46, 0xE9, 0xB6, 0xA1, 0xDA, 0x25, 0xDD,
			};

		AADLEN = sizeof_u32(AAD);

		GMAC_AES_TEST("IEEE/GMAC", COUNT, MAC, AAD, AADLEN, Tag, Taglen);
	}

	LOG_INFO("[GMAC-AES-128 tests: PASSED]\n");
fail:
	return ret;
}

status_t run_aes_gcm_tests(crypto_context_t *crypto_ctx, engine_id_t engine_id)
{
	status_t ret = NO_ERROR;

	ret = run_aes_128_gcm_tests(crypto_ctx, engine_id);
	CCC_ERROR_CHECK(ret,
			LOG_ERROR("[AES-128-GCM tests: FAILED (%d)]\n", ret));

	ret = run_gmac_aes_128_tests(crypto_ctx, engine_id);
	CCC_ERROR_CHECK(ret,
			LOG_ERROR("[GMAC-AES-128 tests: FAILED (%d)]\n", ret));

	LOG_INFO("[AES-GCM tests: PASSED]\n");
fail:
	return ret;
}

#endif /* HAVE_AES_GCM */
#endif /* KERNEL_TEST_MODE */
