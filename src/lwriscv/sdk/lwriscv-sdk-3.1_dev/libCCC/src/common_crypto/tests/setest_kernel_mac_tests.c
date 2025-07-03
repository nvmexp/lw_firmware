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

#if HAVE_CMAC_AES || HAVE_HMAC_SHA
static te_args_key_data_t work_akey;
#endif

#if HAVE_CMAC_AES

static const uint8_t zero_block[16] = { 0x00, };

/* Save some static memory by re-using this key object template
 * with the work_akey
 */
static const te_args_key_data_t akey_template = {
	.k_key_type = KEY_TYPE_AES,
	.k_flags = KEY_FLAG_PLAIN,
	.k_keyslot = AES_KEYSLOT,
	.k_byte_size = 16U,
	.k_aes_key = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff },
};

/*
  echo -n "abcdefghijklmnop" | openssl dgst -mac cmac -macopt cipher:aes-128-cbc -macopt hexkey:000000000000000000000000000000FF -sha1
  (stdin)= 1af70d3928eb1d920deb9a47c27a3855
*/
static status_t TEST_cmac(crypto_context_t *c, te_crypto_algo_t mac, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	unsigned char data[16] = "abcdefghijklmnop";
	uint8_t dst[16+34]; // just for testing output size corrected
	static uint8_t correct_cmac_aes128[] = { 0x1a, 0xf7, 0x0d, 0x39, 0x28, 0xeb, 0x1d, 0x92,
					  0x0d, 0xeb, 0x9a, 0x47, 0xc2, 0x7a, 0x38, 0x55,
	};

	CCC_OBJECT_ASSIGN(work_akey, akey_template);

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, mac);

	if (mac != TE_ALG_CMAC_AES) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	se_util_mem_set(dst, 0U, sizeof_u32(dst));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = TE_ALG_CMAC_AES;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for CMAC AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	arg.ca_set_key.kdata = &work_akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	LOG_INFO("CMAC-AES128(handle %u): result ret %d\n",
		 arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	DUMP_HEX("CMAC-AES-128:", dst, 16U);
	TRAP_ASSERT(arg.ca_data.dst_size == 16U);
	VERIFY_ARRAY_VALUE(dst, correct_cmac_aes128, 16U);

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

#if HAVE_CMAC_DST_KEYTABLE

static status_t TEST_cmac_key_derive128(crypto_context_t *c, te_crypto_algo_t mac, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };
	uint32_t dst_keyslot = 10U;
	unsigned char data[16] = "abcdefghijklmnop";
	uint8_t dst[16];

#if 0
	/* Result of CMAC-AES is written to keyslot 10 and then used
	 * as a cipher key to indirectly verify key derivation result
	 * with the cipher result.
	 */
	static const uint8_t correct_cmac_aes128[] = { 0x1a, 0xf7, 0x0d, 0x39, 0x28, 0xeb, 0x1d, 0x92,
						0x0d, 0xeb, 0x9a, 0x47, 0xc2, 0x7a, 0x38, 0x55,
	};
#endif

	static const uint8_t correct_ecb128_value[] = {
		0x03, 0x97, 0xc6, 0x94, 0xde, 0x08, 0x28, 0x02,
		0x50, 0x47, 0xda, 0x08, 0x88, 0x17, 0xde, 0xb1
	};

	CCC_OBJECT_ASSIGN(work_akey, akey_template);

	/* Clear AES keyslot 10
	 */
	ret = se_clear_device_aes_keyslot(SE_CDEV_SE0, dst_keyslot);
	CCC_ERROR_CHECK(ret,
			LOG_ERROR("Failed to clear aes keyslot %u\n", dst_keyslot));

	/* Static variable: reset fields modified below
	 */
	work_akey.k_flags = KEY_FLAG_PLAIN;
	work_akey.k_keyslot = AES_KEYSLOT;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, mac);

	if (mac != TE_ALG_CMAC_AES) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	se_util_mem_set(dst, 0U, sizeof_u32(dst));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = TE_ALG_CMAC_AES;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg.ca_init.engine_hint = eid;

	/* CMAC-AES final target is keyslot 10 */
	arg.ca_init.mac.mac_flags = INIT_FLAG_MAC_DST_KEYSLOT | INIT_FLAG_MAC_DST_KEY128;
	arg.ca_init.mac.mac_dst_keyslot = dst_keyslot;

	arg.ca_set_key.kdata = &work_akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;

	/* CMAC outputs 16 bytes to keyslot; nothing written to memory
	 */
	arg.ca_data.dst_size = 0U;
	arg.ca_data.dst = NULL;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	LOG_INFO("CMAC-AES128(handle %u): result ret %d\n",
		 arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	/* This is set in the call: 16 bytes written to keyslot
	 */
	TRAP_ASSERT(arg.ca_data.dst_size == 16U);

	CRYPTO_CONTEXT_RESET(c);

	/* Prepare the work_akey object to use an now existing 128 bit
	 * key from keyslot 10 (derived with the CMAC-AES above)
	 */
	work_akey.k_flags = KEY_FLAG_LEAVE_KEY_IN_KEYSLOT | KEY_FLAG_USE_KEYSLOT_KEY;
	work_akey.k_keyslot = dst_keyslot;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, TE_ALG_AES_ECB_NOPAD);

	se_util_mem_set((uint8_t *)&arg, 0U, sizeof_u32(arg));

	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = TE_ALG_AES_ECB_NOPAD;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	arg.ca_set_key.kdata = &work_akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;

	/* AES-128-ECB ciphers one block from data to dst
	 */
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	/* Verify result of AES-ECB<cmac-derived-key>(data)
	 */
	TRAP_ASSERT(arg.ca_data.dst_size == 16U);
	DUMP_HEX("ECB CIPHER RESULT (src == dst):", dst, 16U);
	VERIFY_ARRAY_VALUE(dst, correct_ecb128_value, sizeof_u32(correct_ecb128_value));
fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

static status_t TEST_cmac_key_derive256(crypto_context_t *c, te_crypto_algo_t mac, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };
	uint32_t dst_keyslot = 11U;
	unsigned char data[16]  = "abcdefghijklmnop";
	unsigned char data2[16] = "ABCDEFGHIJKLMNOP";
	uint8_t dst[16];

#if 0
	/* Result of CMAC-AES is written to keyslot 10 and then used
	 * as a cipher key to indirectly verify key derivation result
	 * with the cipher result.
	 */
	static const uint8_t correct_cmac_aes256[] = { 0x1a, 0xf7, 0x0d, 0x39, 0x28, 0xeb, 0x1d, 0x92,
						0x0d, 0xeb, 0x9a, 0x47, 0xc2, 0x7a, 0x38, 0x55,

						/* Upper key material bits 128..255
						 * CMAC-AES(akey,data2)
						 */
						0x0c, 0x67, 0xec, 0xf4, 0x31, 0xa2, 0x98, 0x05,
						0x47, 0x67, 0x53, 0x5f, 0xde, 0x57, 0x0b, 0xd9,
	};
#endif

	static const uint8_t correct_ecb256_value[] = {
		0x05, 0x75, 0x32, 0xdc, 0x2e, 0xf6, 0x4e, 0x80,
		0x80, 0x22, 0x91, 0x26, 0xc3, 0xa7, 0x48, 0x03,
	};

	CCC_OBJECT_ASSIGN(work_akey, akey_template);

	/* Clear AES keyslot 11
	 */
	ret = se_clear_device_aes_keyslot(SE_CDEV_SE0, dst_keyslot);
	CCC_ERROR_CHECK(ret,
			LOG_ERROR("Failed to clear aes keyslot %u\n", dst_keyslot));

	/* Static variable: reset fields modified below
	 */
	work_akey.k_flags = KEY_FLAG_PLAIN;
	work_akey.k_keyslot = AES_KEYSLOT;
	work_akey.k_byte_size = 16U;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, mac);

	if (mac != TE_ALG_CMAC_AES) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	se_util_mem_set(dst, 0U, sizeof_u32(dst));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = TE_ALG_CMAC_AES;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg.ca_init.engine_hint = eid;

	/* CMAC-AES final target is keyslot 11 */
	arg.ca_init.mac.mac_flags = INIT_FLAG_MAC_DST_KEYSLOT | INIT_FLAG_MAC_DST_KEY256;
	arg.ca_init.mac.mac_dst_keyslot = dst_keyslot;

	arg.ca_set_key.kdata = &work_akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;

	/* CMAC outputs 16 bytes to keyslot bits 0..127;
	 * nothing written to memory
	 */
	arg.ca_data.dst_size = 0U;
	arg.ca_data.dst = NULL;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	LOG_INFO("CMAC-AES128(handle %u): result ret %d\n",
		 arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	/* This is set in the call: 16 bytes written to keyslot bits 0..127;
	 */
	TRAP_ASSERT(arg.ca_data.dst_size == 16U);

	CRYPTO_CONTEXT_RESET(c);

	/* Next write the upper keyslot bits 128..255
	 */
	arg.ca_init.mac.mac_flags =
		INIT_FLAG_MAC_DST_KEYSLOT | INIT_FLAG_MAC_DST_KEY256 | INIT_FLAG_MAC_DST_UPPER_QUAD;

	arg.ca_data.src_size = sizeof_u32(data2);
	arg.ca_data.src = data2;

	/* CMAC outputs 16 bytes to keyslot (bits 128..255);
	 * nothing written to memory
	 */
	arg.ca_data.dst_size = 0U;
	arg.ca_data.dst = NULL;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	LOG_INFO("CMAC-AES128(handle %u): result ret %d\n",
		 arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	TRAP_ASSERT(arg.ca_data.dst_size == 16U);

	CRYPTO_CONTEXT_RESET(c);

	/* Prepare the work_akey object to use an now existing 256 bit
	 * key from keyslot 11 (derived with the two CMAC-AES ops above)
	 */
	work_akey.k_flags = KEY_FLAG_LEAVE_KEY_IN_KEYSLOT | KEY_FLAG_USE_KEYSLOT_KEY;
	work_akey.k_keyslot = dst_keyslot;
	work_akey.k_byte_size = 32U;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, TE_ALG_AES_ECB_NOPAD);

	se_util_mem_set((uint8_t *)&arg, 0U, sizeof_u32(arg));

	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = TE_ALG_AES_ECB_NOPAD;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	arg.ca_set_key.kdata = &work_akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;

	/* AES-256-ECB ciphers one block from data to dst
	 */
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	/* Verify result of AES-ECB<cmac-derived-key>(data)
	 */
	TRAP_ASSERT(arg.ca_data.dst_size == 16U);
	DUMP_HEX("ECB-256 CIPHER RESULT (src == dst):", dst, 16U);
	VERIFY_ARRAY_VALUE(dst, correct_ecb256_value, sizeof_u32(correct_ecb256_value));
fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

static status_t TEST_cmac_key_derive128_long(crypto_context_t *c, te_crypto_algo_t mac, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };
	uint32_t dst_keyslot = 10U;
	unsigned char data[48] = "abcdefghijklmnopabcdefghijklmnopabcdefghijklmnop";
	uint8_t dst[16];

#if 0
	/* Result of CMAC-AES is written to keyslot 10 and then used
	 * as a cipher key to indirectly verify key derivation result
	 * with the cipher result.
	 */
	static const uint8_t correct_cmac_aes128[] = {
		0x15, 0x23, 0x06, 0x15, 0x10, 0x39, 0x06, 0xf8,
		0xdb, 0xfd, 0xc0, 0x18, 0x0c, 0x83, 0xc3, 0x09,
	};
#endif

	static const uint8_t correct_ecb128_value[] = {
		0xb9, 0xde, 0x55, 0xd1, 0xa6, 0x06, 0x01, 0x71,
		0x13, 0x36, 0xab, 0xe7, 0x10, 0x79, 0x7f, 0x63,
	};

	CCC_OBJECT_ASSIGN(work_akey, akey_template);

	/* Clear AES keyslot 10
	 */
	ret = se_clear_device_aes_keyslot(SE_CDEV_SE0, dst_keyslot);
	CCC_ERROR_CHECK(ret,
			LOG_ERROR("Failed to clear aes keyslot %u\n", dst_keyslot));

	/* Static variable: reset fields modified below
	 */
	work_akey.k_flags = KEY_FLAG_PLAIN;
	work_akey.k_keyslot = AES_KEYSLOT;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, mac);

	if (mac != TE_ALG_CMAC_AES) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	se_util_mem_set(dst, 0U, sizeof_u32(dst));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = TE_ALG_CMAC_AES;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg.ca_init.engine_hint = eid;

	/* CMAC-AES final target is keyslot 10 */
	arg.ca_init.mac.mac_flags = INIT_FLAG_MAC_DST_KEYSLOT | INIT_FLAG_MAC_DST_KEY128;
	arg.ca_init.mac.mac_dst_keyslot = dst_keyslot;

	arg.ca_set_key.kdata = &work_akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;

	/* CMAC outputs 16 bytes to keyslot; nothing written to memory
	 */
	arg.ca_data.dst_size = 0U;
	arg.ca_data.dst = NULL;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	LOG_INFO("CMAC-AES128(handle %u): result ret %d\n",
		 arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	/* This is set in the call: 16 bytes written to keyslot
	 */
	TRAP_ASSERT(arg.ca_data.dst_size == 16U);

	CRYPTO_CONTEXT_RESET(c);

	/* Prepare the akey object to use an now existing 128 bit
	 * key from keyslot 10 (derived with the CMAC-AES above)
	 */
	work_akey.k_flags = KEY_FLAG_LEAVE_KEY_IN_KEYSLOT | KEY_FLAG_USE_KEYSLOT_KEY;
	work_akey.k_keyslot = dst_keyslot;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, TE_ALG_AES_ECB_NOPAD);

	se_util_mem_set((uint8_t *)&arg, 0U, sizeof_u32(arg));

	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = TE_ALG_AES_ECB_NOPAD;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	arg.ca_set_key.kdata = &work_akey;

	arg.ca_data.src_size = sizeof_u32(zero_block);
	arg.ca_data.src = zero_block;

	/* AES-128-ECB ciphers one block from data to dst
	 */
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	/* Verify result of AES-ECB<cmac-derived-key>(data)
	 */
	TRAP_ASSERT(arg.ca_data.dst_size == 16U);
	DUMP_HEX("ECB CIPHER RESULT (src == dst):", dst, 16U);
	VERIFY_ARRAY_VALUE(dst, correct_ecb128_value, sizeof_u32(correct_ecb128_value));
fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

static status_t TEST_cmac_key_derive256_long(crypto_context_t *c, te_crypto_algo_t mac, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };
	uint32_t dst_keyslot = 11U;
	unsigned char data[36]  = "abcdefghijklmnop00010203040506070809"; // 16+20 == 36 bytes
	unsigned char data2[18] = "ABCDEFGHIJKLMNOP01"; // 16 + 2 == 18 bytes
	uint8_t dst[16];

#if 0
	/* Result of CMAC-AES is written to keyslot 10 and then used
	 * as a cipher key to indirectly verify key derivation result
	 * with the cipher result.
	 */
	static const uint8_t correct_cmac_aes256[] = {
		/* Lower key material bits 0..127
		 * CMAC-AES(akey,data)
		 */
		0x26, 0x97, 0x3d, 0xbc, 0xe1, 0xc4, 0xe8, 0x60,
		0xf1, 0x6e, 0x37, 0x1d, 0xe8, 0xdb, 0x9f, 0x69,

		/* Upper key material bits 128..255
		 * CMAC-AES(akey,data2)
		 */
		0xbe, 0x94, 0x13, 0x9f, 0x85, 0xf9, 0x5c, 0xf8,
		0x7a, 0xfa, 0x78, 0x29, 0xe3, 0x2c, 0xeb, 0xab,
	};
#endif

	static const uint8_t correct_ecb256_value[] = {
		0x0b, 0x54, 0xee, 0x55, 0x1f, 0x0e, 0x97, 0x1b,
		0xbc, 0x08, 0xc9, 0xc3, 0x87, 0x36, 0xa6, 0xea,
	};

	CCC_OBJECT_ASSIGN(work_akey, akey_template);

	/* Clear AES keyslot 11
	 */
	ret = se_clear_device_aes_keyslot(SE_CDEV_SE0, dst_keyslot);
	CCC_ERROR_CHECK(ret,
			LOG_ERROR("Failed to clear aes keyslot %u\n", dst_keyslot));

	/* Static variable: reset fields modified below
	 */
	work_akey.k_flags = KEY_FLAG_PLAIN;
	work_akey.k_keyslot = AES_KEYSLOT;
	work_akey.k_byte_size = 16U;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, mac);

	if (mac != TE_ALG_CMAC_AES) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	se_util_mem_set(dst, 0U, sizeof_u32(dst));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = TE_ALG_CMAC_AES;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg.ca_init.engine_hint = eid;

	/* CMAC-AES final target is keyslot 11 */
	arg.ca_init.mac.mac_flags = INIT_FLAG_MAC_DST_KEYSLOT | INIT_FLAG_MAC_DST_KEY256;
	arg.ca_init.mac.mac_dst_keyslot = dst_keyslot;

	arg.ca_set_key.kdata = &work_akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;

	/* CMAC outputs 16 bytes to keyslot bits 0..127;
	 * nothing written to memory
	 */
	arg.ca_data.dst_size = 0U;
	arg.ca_data.dst = NULL;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	LOG_INFO("CMAC-AES128(handle %u): result ret %d\n",
		 arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	/* This is set in the call: 16 bytes written to keyslot bits 0..127;
	 */
	TRAP_ASSERT(arg.ca_data.dst_size == 16U);

	CRYPTO_CONTEXT_RESET(c);

	/* Next write the upper keyslot bits 128..255
	 */
	arg.ca_init.mac.mac_flags =
		INIT_FLAG_MAC_DST_KEYSLOT | INIT_FLAG_MAC_DST_KEY256 | INIT_FLAG_MAC_DST_UPPER_QUAD;

	arg.ca_data.src_size = sizeof_u32(data2);
	arg.ca_data.src = data2;

	/* CMAC outputs 16 bytes to keyslot (bits 128..255);
	 * nothing written to memory
	 */
	arg.ca_data.dst_size = 0U;
	arg.ca_data.dst = NULL;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	LOG_INFO("CMAC-AES128(handle %u): result ret %d\n",
		 arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	TRAP_ASSERT(arg.ca_data.dst_size == 16U);

	CRYPTO_CONTEXT_RESET(c);

	/* Prepare the work_akey object to use an now existing 256 bit
	 * key from keyslot 11 (derived with the two CMAC-AES ops above)
	 */
	work_akey.k_flags = KEY_FLAG_LEAVE_KEY_IN_KEYSLOT | KEY_FLAG_USE_KEYSLOT_KEY;
	work_akey.k_keyslot = dst_keyslot;
	work_akey.k_byte_size = 32U;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, TE_ALG_AES_ECB_NOPAD);

	se_util_mem_set((uint8_t *)&arg, 0U, sizeof_u32(arg));

	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = TE_ALG_AES_ECB_NOPAD;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	arg.ca_set_key.kdata = &work_akey;

	arg.ca_data.src_size = sizeof_u32(zero_block);
	arg.ca_data.src = zero_block;

	/* AES-256-ECB ciphers one block from zero_block to dst
	 */
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	/* Verify result of AES-ECB<cmac-derived-key>(data)
	 */
	TRAP_ASSERT(arg.ca_data.dst_size == 16U);
	DUMP_HEX("ECB-256 CIPHER RESULT (src == dst):", dst, 16U);
	VERIFY_ARRAY_VALUE(dst, correct_ecb256_value, sizeof_u32(correct_ecb256_value));
fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

static status_t TEST_cmac_key_derive128_preset_key(crypto_context_t *c, te_crypto_algo_t mac, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };
	uint32_t dst_keyslot = 10U;
	unsigned char data[16] = "abcdefghijklmnop";
	uint8_t dst[16];
	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = 16,
	};

	static uint8_t derivation_key[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff };


#if 0
	/* Result of CMAC-AES is written to keyslot 10 and then used
	 * as a cipher key to indirectly verify key derivation result
	 * with the cipher result.
	 */
	const uint8_t correct_cmac_aes128[] = { 0x1a, 0xf7, 0x0d, 0x39, 0x28, 0xeb, 0x1d, 0x92,
						0x0d, 0xeb, 0x9a, 0x47, 0xc2, 0x7a, 0x38, 0x55,
	};
#endif

	static const uint8_t correct_ecb128_value[] = {
		0x03, 0x97, 0xc6, 0x94, 0xde, 0x08, 0x28, 0x02,
		0x50, 0x47, 0xda, 0x08, 0x88, 0x17, 0xde, 0xb1
	};

	/* Clear AES keyslot 10
	 */
	ret = se_clear_device_aes_keyslot(SE_CDEV_SE0, dst_keyslot);
	CCC_ERROR_CHECK(ret,
			LOG_ERROR("Failed to clear aes keyslot %u\n", dst_keyslot));

	/* Set the derivation AES key into AES_KEYSLOT (2)
	 */
	ret = se_set_device_aes_keyslot(SE_CDEV_SE0, AES_KEYSLOT,
					derivation_key, 128U);
	CCC_ERROR_CHECK(ret);

	/* Static variable: reset fields modified below
	 */
	akey.k_flags = KEY_FLAG_LEAVE_KEY_IN_KEYSLOT | KEY_FLAG_USE_KEYSLOT_KEY;
	akey.k_keyslot = AES_KEYSLOT;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, mac);

	if (mac != TE_ALG_CMAC_AES) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	se_util_mem_set(dst, 0U, sizeof_u32(dst));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = TE_ALG_CMAC_AES;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg.ca_init.engine_hint = eid;

	/* CMAC-AES final target is keyslot 10 */
	arg.ca_init.mac.mac_flags = INIT_FLAG_MAC_DST_KEYSLOT | INIT_FLAG_MAC_DST_KEY128;
	arg.ca_init.mac.mac_dst_keyslot = dst_keyslot;

	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;

	/* CMAC outputs 16 bytes to keyslot; nothing written to memory
	 */
	arg.ca_data.dst_size = 0U;
	arg.ca_data.dst = NULL;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	LOG_INFO("CMAC-AES128(handle %u): result ret %d\n",
		 arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	/* This is set in the call: 16 bytes written to keyslot
	 */
	TRAP_ASSERT(arg.ca_data.dst_size == 16U);

	CRYPTO_CONTEXT_RESET(c);

	/* Prepare the akey object to use an now existing 128 bit
	 * key from keyslot 10 (derived with the CMAC-AES above)
	 */
	akey.k_flags = KEY_FLAG_LEAVE_KEY_IN_KEYSLOT | KEY_FLAG_USE_KEYSLOT_KEY;
	akey.k_keyslot = dst_keyslot;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, TE_ALG_AES_ECB_NOPAD);

	se_util_mem_set((uint8_t *)&arg, 0U, sizeof_u32(arg));

	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = TE_ALG_AES_ECB_NOPAD;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;

	/* AES-128-ECB ciphers one block from data to dst
	 */
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	/* Verify result of AES-ECB<cmac-derived-key>(data)
	 */
	TRAP_ASSERT(arg.ca_data.dst_size == 16U);
	DUMP_HEX("ECB CIPHER RESULT (src == dst):", dst, 16U);
	VERIFY_ARRAY_VALUE(dst, correct_ecb128_value, sizeof_u32(correct_ecb128_value));
fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

static status_t TEST_cmac_key_derive256_preset_key(crypto_context_t *c, te_crypto_algo_t mac, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };
	uint32_t dst_keyslot = 11U;
	unsigned char data[16]  = "abcdefghijklmnop";
	unsigned char data2[16] = "ABCDEFGHIJKLMNOP";
	uint8_t dst[16];
	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_LEAVE_KEY_IN_KEYSLOT | KEY_FLAG_USE_KEYSLOT_KEY,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = 16U,
		/* Key material set separately */
	};

	static uint8_t derivation_key[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff };

#if 0
	/* Result of CMAC-AES is written to keyslot 10 and then used
	 * as a cipher key to indirectly verify key derivation result
	 * with the cipher result.
	 */
	const uint8_t correct_cmac_aes256[] = { 0x1a, 0xf7, 0x0d, 0x39, 0x28, 0xeb, 0x1d, 0x92,
						0x0d, 0xeb, 0x9a, 0x47, 0xc2, 0x7a, 0x38, 0x55,

						/* Upper key material bits 128..255
						 * CMAC-AES(akey,data2)
						 */
						0x0c, 0x67, 0xec, 0xf4, 0x31, 0xa2, 0x98, 0x05,
						0x47, 0x67, 0x53, 0x5f, 0xde, 0x57, 0x0b, 0xd9,
	};
#endif

	static const uint8_t correct_ecb256_value[] = {
		0x05, 0x75, 0x32, 0xdc, 0x2e, 0xf6, 0x4e, 0x80,
		0x80, 0x22, 0x91, 0x26, 0xc3, 0xa7, 0x48, 0x03,
	};

	/* Clear AES keyslot 11
	 */
	ret = se_clear_device_aes_keyslot(SE_CDEV_SE0, dst_keyslot);
	CCC_ERROR_CHECK(ret,
			LOG_ERROR("Failed to clear aes keyslot %u\n", dst_keyslot));

	/* Set the derivation AES key into AES_KEYSLOT (2)
	 */
	ret = se_set_device_aes_keyslot(SE_CDEV_SE0, AES_KEYSLOT,
					derivation_key, 128U);
	CCC_ERROR_CHECK(ret);

	/* Static variable: reset fields modified below
	 */
	akey.k_flags = KEY_FLAG_LEAVE_KEY_IN_KEYSLOT | KEY_FLAG_USE_KEYSLOT_KEY;
	akey.k_keyslot = AES_KEYSLOT;
	akey.k_byte_size = 16U;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, mac);

	if (mac != TE_ALG_CMAC_AES) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	se_util_mem_set(dst, 0U, sizeof_u32(dst));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = TE_ALG_CMAC_AES;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg.ca_init.engine_hint = eid;

	/* CMAC-AES final target is keyslot 11 */
	arg.ca_init.mac.mac_flags = INIT_FLAG_MAC_DST_KEYSLOT | INIT_FLAG_MAC_DST_KEY256;
	arg.ca_init.mac.mac_dst_keyslot = dst_keyslot;

	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;

	/* CMAC outputs 16 bytes to keyslot bits 0..127;
	 * nothing written to memory
	 */
	arg.ca_data.dst_size = 0U;
	arg.ca_data.dst = NULL;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	LOG_INFO("CMAC-AES128(handle %u): result ret %d\n",
		 arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	/* This is set in the call: 16 bytes written to keyslot bits 0..127;
	 */
	TRAP_ASSERT(arg.ca_data.dst_size == 16U);

	CRYPTO_CONTEXT_RESET(c);

	/* Next write the upper keyslot bits 128..255
	 */
	arg.ca_init.mac.mac_flags =
		INIT_FLAG_MAC_DST_KEYSLOT | INIT_FLAG_MAC_DST_KEY256 | INIT_FLAG_MAC_DST_UPPER_QUAD;

	arg.ca_data.src_size = sizeof_u32(data2);
	arg.ca_data.src = data2;

	/* CMAC outputs 16 bytes to keyslot (bits 128..255);
	 * nothing written to memory
	 */
	arg.ca_data.dst_size = 0U;
	arg.ca_data.dst = NULL;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	LOG_INFO("CMAC-AES128(handle %u): result ret %d\n",
		 arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	TRAP_ASSERT(arg.ca_data.dst_size == 16U);

	CRYPTO_CONTEXT_RESET(c);

	/* Prepare the akey object to use an now existing 256 bit
	 * key from keyslot 11 (derived with the two CMAC-AES ops above)
	 */
	akey.k_flags = KEY_FLAG_LEAVE_KEY_IN_KEYSLOT | KEY_FLAG_USE_KEYSLOT_KEY;
	akey.k_keyslot = dst_keyslot;
	akey.k_byte_size = 32U;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_ENCRYPT, TE_ALG_AES_ECB_NOPAD);

	se_util_mem_set((uint8_t *)&arg, 0U, sizeof_u32(arg));

	arg.ca_alg_mode = TE_ALG_MODE_ENCRYPT;
	arg.ca_algo     = TE_ALG_AES_ECB_NOPAD;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;

	/* AES-256-ECB ciphers one block from data to dst
	 */
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	/* Verify result of AES-ECB<cmac-derived-key>(data)
	 */
	TRAP_ASSERT(arg.ca_data.dst_size == 16U);
	DUMP_HEX("ECB-256 CIPHER RESULT (src == dst):", dst, 16U);
	VERIFY_ARRAY_VALUE(dst, correct_ecb256_value, sizeof_u32(correct_ecb256_value));
fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}
#endif /* HAVE_CMAC_DST_KEYTABLE */

static status_t TEST_cmac_preset_key_64_bytes(crypto_context_t *c, te_crypto_algo_t algo, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	DMA_ALIGN_DATA uint8_t data[64];
	DMA_ALIGN_DATA uint8_t dst[16]; // just for testing output size corrected

	static uint8_t correct_cmac_aes128[] = {
		0xce, 0xe6, 0x2a, 0x49, 0x55, 0x05, 0x07, 0x05,
		0x05, 0x91, 0x27, 0x3b, 0x2a, 0x5a, 0x12, 0x84,
	};

	CCC_OBJECT_ASSIGN(work_akey, akey_template);

	if (algo != TE_ALG_CMAC_AES) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	se_util_mem_set(dst, 0U, sizeof_u32(dst));
	se_util_mem_set(data, 'A', sizeof_u32(data));

	// Leave the key in keyslot after setting it for the next test
	work_akey.k_flags = KEY_FLAG_LEAVE_KEY_IN_KEYSLOT;

	LOG_ALWAYS("Set AES-128 key to keyslot and perform CMAC_AES\n");

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, algo);

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = TE_ALG_CMAC_AES;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg.ca_init.engine_hint = eid;

	arg.ca_set_key.kdata = &work_akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	CCC_ERROR_CHECK(ret);

	DUMP_HEX("CMAC-AES-128:", dst, 16U);
	TRAP_ASSERT(arg.ca_data.dst_size == 16U);
	VERIFY_ARRAY_VALUE(dst, correct_cmac_aes128, 16U);

	se_util_mem_set((uint8_t *)&arg, 0U, sizeof_u32(arg));
	se_util_mem_set(dst, 0U, sizeof_u32(dst));

	LOG_ALWAYS("Use existing AES-128 key in keyslot and perform CMAC_AES\n");
	work_akey.k_flags = KEY_FLAG_USE_KEYSLOT_KEY | KEY_FLAG_LEAVE_KEY_IN_KEYSLOT;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, algo);

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = TE_ALG_CMAC_AES;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg.ca_init.engine_hint = eid;

	arg.ca_set_key.kdata = &work_akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	CCC_ERROR_CHECK(ret);

	DUMP_HEX("CMAC-AES-128:", dst, 16U);
	TRAP_ASSERT(arg.ca_data.dst_size == 16U);
	VERIFY_ARRAY_VALUE(dst, correct_cmac_aes128, 16U);

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

// Same data as in TEST_cmac_with_update_byte(), just done in
// one call.
static status_t TEST_cmac_one_pass(crypto_context_t *c, te_crypto_algo_t mac, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	uint8_t dst[16];
	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = 16,
		.k_aes_key = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			       0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f },
	};

	/*
	  Subkey Generation : correct values
	  AES_128(key,0) c6a13b37 878f5b82 6f4f8162 a1c8d879
	  K1             8d42766f 0f1eb704 de9f02c5 4391b075
	  K2             1a84ecde 1e3d6e09 bd3e058a 8723606d
	*/

	static unsigned char data[] =
		"!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmno!" ;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, mac);

	/* Preset fields, proceed up to key setting */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = mac;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for CMAC AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src = data;
	arg.ca_data.src_size = 80;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	LOG_INFO("MAC: ALGO: 0x%x, ret %d, attached handle: %u\n",
		   mac, ret, arg.ca_handle);
	if (NO_ERROR != ret) {
		TRAP_ERROR("syscall failed\n");
	}

	if (arg.ca_data.dst_size != 16) {
		TRAP_ERROR("CMAC result length incorrect\n");
	}

	DUMP_HEX("CMAC-AES-128 (full):", dst, sizeof_u32(dst));

	/* verify the result:
	 * cat data | openssl dgst -mac cmac -macopt cipher:aes-128-cbc
	 *          -macopt hexkey:000102030405060708090a0b0c0d0e0f -sha1
	 */

	static uint8_t correct[16] = {
		0x0a, 0xfb, 0x72, 0x12, 0x3d, 0x28, 0xe8, 0xf4,
		0xcb, 0x48, 0xcc, 0x66, 0xff, 0xb2, 0x39, 0x7c,
	};

	// the results must match
	VERIFY_ARRAY_VALUE(dst, correct, 16U);

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}


/* test CMAC-AES by passing first 5*16 bytes to it one byte at the time.
 * Then pass 5*16 bytes to it in one call; compare the results (with previous).
 * Then pass 5*16 bytes to it in in 5 calls of 16 bytes; compare the results.
 */
static status_t TEST_cmac_with_update_byte(crypto_context_t *c, te_crypto_algo_t mac, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	uint8_t first_result[16];
	uint8_t dst[17];
	uint8_t byte;
	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = 16,
		.k_aes_key = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			       0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f },
	};
#define BLOCKS 5
	// UPDATE with single bytes, values '!'..('!'+(16*BLOCKS)-1) + '!') inclusive
	// update(&byte);
	//
	static uint8_t data[16*BLOCKS+1];
	unsigned int i = 0;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, mac);

	se_util_mem_set(data, 0U, sizeof_u32(data));

	/* Preset fields, proceed up to key setting */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = mac;
	arg.ca_opcode   = op_INIT_KEY;

	LOG_INFO("Hint: use engine 0x%x (%s) for CMAC AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	arg.ca_set_key.kdata = &akey;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	LOG_INFO("MAC(alloc-key): ALGO: 0x%x, ret %d, attached handle: %u\n",
		   mac, ret, arg.ca_handle);
	if (NO_ERROR != ret) {
		TRAP_ERROR("syscall failed\n");
	}

	// These are not used for CMAC updates...
	// (values ignored)
	arg.ca_data.dst_size = 0;
	arg.ca_data.dst = NULL;

	/* Populate the data[] array with the src for cmac for subsequent tests */
	for (; i < (16*BLOCKS)-1; i++) {
		byte = BYTE_VALUE('!' + i);
		data[i] = byte;
		arg.ca_opcode = TE_OP_UPDATE;

		arg.ca_data.src_size = sizeof_u32(byte);
		arg.ca_data.src = &byte;

		ret = CRYPTO_OPERATION(c, &arg);
		if (NO_ERROR != ret) {
			TRAP_ERROR("MAC(update) byte %c, handle %u: ret %d\n",
					byte, arg.ca_handle,ret);
		}
	}

	arg.ca_opcode = op_FINAL_REL;

	byte = '!';
	data[i++] = byte;

	// the terminating NUL is not part of CMAC input, Just outputting as string...
	data[i] = '\000';

	LOG_ALWAYS("Combined data to CMAC: '%s'\n", data);

	arg.ca_data.src_size = sizeof_u32(byte);
	arg.ca_data.src = &byte;
	arg.ca_data.dst_size = sizeof_u32(first_result);
	arg.ca_data.dst = first_result;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("MAC(dofinal-release) handle %u: ret %d\n",
		 arg.ca_handle,ret);
	if (NO_ERROR != ret) {
		TRAP_ERROR("syscall failed\n");
	}

	if (arg.ca_data.dst_size != 16) {
		TRAP_ERROR("CMAC result length incorrect\n");
	}

	DUMP_HEX("CMAC-AES-128 (byte updates):", first_result, sizeof_u32(first_result));

	/* RETEST with a single CMAC call => dofinal(data) */

	CRYPTO_CONTEXT_RESET(c);
	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, mac);

	se_util_mem_set((uint8_t *)&arg, 0U, sizeof_u32(arg));
	se_util_mem_set(dst, 0U, sizeof_u32(dst));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = mac;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for CMAC AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = 16 * BLOCKS;
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(dst); // Passing one size bigger than required...
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	LOG_INFO("retest: MAC(combined): ALGO: 0x%x, ret %d, attached handle: %u\n",
		   mac, ret, arg.ca_handle);
	CCC_ERROR_CHECK(ret);

	if (arg.ca_data.dst_size != 16) {
		TRAP_ERROR("CMAC result length incorrect\n");
	}

	DUMP_HEX("CMAC-AES-128 (80 bytes at once):", dst, 16);

	// the results must match
	VERIFY_ARRAY_VALUE(dst, first_result, 16U);

	/* This si hte correct value, must also match that ;-) */
	static uint8_t correct[16] = {
		0x0a, 0xfb, 0x72, 0x12, 0x3d, 0x28, 0xe8, 0xf4,
		0xcb, 0x48, 0xcc, 0x66, 0xff, 0xb2, 0x39, 0x7c,
	};

	// the callwlated results must match with the correct value
	VERIFY_ARRAY_VALUE(dst, correct, 16U);

	/* RETEST with a one call per 16 byte data chunk CMAC call =>
	 * update(BLOCKS-1) + dofinal(LAST_BLOCK)
	 */
	se_util_mem_set((uint8_t *)&arg, 0U, sizeof_u32(arg));
	se_util_mem_set(dst, 0U, sizeof_u32(dst));

	CRYPTO_CONTEXT_RESET(c);
	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, mac);

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = mac;
	arg.ca_opcode   = op_INIT_KEY;

	LOG_INFO("Hint: use engine 0x%x (%s) for CMAC AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	arg.ca_set_key.kdata = &akey;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	LOG_INFO("Chunked MAC(allocate): ALGO: 0x%x, ret %d, attached handle: %u\n",
		 mac, ret, arg.ca_handle);
	if (NO_ERROR != ret) {
		TRAP_ERROR("syscall failed\n");
	}

	uint8_t *dptr = data;

	LOG_ALWAYS("Chunked (handle: %u) %u bytes of data (in %u chunks) to CMAC\n",
		   arg.ca_handle, sizeof_u32(data) - 1, BLOCKS);

	for (i = 0; i < (BLOCKS-1); i++) {
		arg.ca_opcode = TE_OP_UPDATE;

		arg.ca_data.src_size = 16;
		arg.ca_data.src = dptr;
		dptr += 16;

		// These are not used for CMAC updates... Verify correct handling
		arg.ca_data.dst = dst;
		arg.ca_data.dst_size = 100;

		ret = CRYPTO_OPERATION(c, &arg);
		if (NO_ERROR != ret) {
			TRAP_ERROR("Chunked MAC(update) 16 bytes, handle %u: ret %d\n",
				   arg.ca_handle,ret);
		}

		if (arg.ca_data.dst_size != 0) {
			TRAP_ERROR("CMAC update returned nonzero dst_size: %u\n",
				   arg.ca_data.dst_size);
		}
	}

	arg.ca_opcode = op_FINAL_REL;

	arg.ca_data.src_size = 16;
	arg.ca_data.src = dptr;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Chunked MAC(dofinal) handle %u: ret %d\n",
		 arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	if (arg.ca_data.dst_size != 16) {
		TRAP_ERROR("CMAC result length incorrect\n");
	}

	DUMP_HEX("CMAC-AES-128 (chunked):", dst, 16);

	// the results must match
	VERIFY_ARRAY_VALUE(dst, first_result, 16U);

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

#define CMAC_CHUNKS 10U

static status_t TEST_cmac_with_update(crypto_context_t *c, te_crypto_algo_t mac, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	uint8_t dst[16];
	static te_args_key_data_t akey = {
		.k_key_type = KEY_TYPE_AES,
		.k_flags = KEY_FLAG_PLAIN,
		.k_keyslot = AES_KEYSLOT,
		.k_byte_size = 16,
		.k_aes_key = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			       0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f },
	};

	uint8_t correct_result[16] = {
		0xba, 0x15, 0x14, 0x8f, 0x9e, 0xc2, 0x05, 0x7a,
		0xb4, 0x2f, 0x18, 0x7a, 0x6d, 0xd0, 0x53, 0x10,
	};
	unsigned int i = 0;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, mac);

	/* Preset fields, proceed up to key setting */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = mac;
	arg.ca_opcode   = op_INIT_KEY;

	LOG_INFO("Hint: use engine 0x%x (%s) for CMAC AES\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	arg.ca_set_key.kdata = &akey;

	ret = CRYPTO_OPERATION(c, &arg);
	CCC_ERROR_CHECK(ret);

	LOG_INFO("Operation uses engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	// These are not used for CMAC updates...
	// (values ignored)
	arg.ca_data.dst_size = 0;
	arg.ca_data.dst = NULL;

	arg.ca_opcode = TE_OP_UPDATE;

	/* Pass 10 zero blocks (160 zero bytes) through CMAC update calls.
	 */
	for (; i < (CMAC_CHUNKS - 1U); i++) {
		arg.ca_data.src_size = sizeof_u32(zero_block);
		arg.ca_data.src = zero_block;

		ret = CRYPTO_OPERATION(c, &arg);
		CCC_ERROR_CHECK(ret);
	}

	arg.ca_opcode = op_FINAL_REL;

	arg.ca_data.src_size = sizeof_u32(zero_block);
	arg.ca_data.src = zero_block;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	CCC_ERROR_CHECK(ret);

	if (arg.ca_data.dst_size != 16U) {
		TRAP_ERROR("CMAC result length incorrect\n");
	}

	DUMP_HEX("CMAC-AES-128 (with updates, result):", dst, 16U);

	// the results must match
	VERIFY_ARRAY_VALUE(dst, correct_result, 16U);

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

#ifdef TEST_SEC_SAFE_REQ_VERIFY
status_t tegra_se_aes_cmac_hw_reg_clear_verify(engine_id_t eid);

static status_t TEST_cmac_aes_clr_hw_regs_verify(crypto_context_t *c, te_crypto_algo_t algo, engine_id_t eid)
{
	status_t ret = NO_ERROR;

	(void)algo;

	/* Verfiy the CCC clearing the HW registers after crypto operations
	 * SE0 has the following HW registers storing result
	 * - SE0_AES0_CMAC_RESULT_0
	 * - SE0_AES1_CMAC_RESULT_0
	 */

	/* Regular CMAC-AES test */
	TEST_cmac_one_pass(c, TE_ALG_CMAC_AES, eid);
	CCC_ERROR_CHECK(ret);

	/* Verify SE0_AESx_CMAC_RESULT_0 is cleared */
	ret = tegra_se_aes_cmac_hw_reg_clear_verify(eid);
	CCC_ERROR_CHECK(ret);

fail:
	return ret;
}
#endif /* TEST_SEC_SAFE_REQ_VERIFY */
#endif /* HAVE_CMAC_AES */

#if HAVE_HMAC_SHA || HAVE_CMAC_AES
/* Openssl command to callwlate NULL HMACs for SHA and MD5 (example: sha1) for this test
 *
 * < /dev/null openssl dgst -sha1 -mac HMAC -macopt "hexkey:00000000000000000000000000000000"
 *
 */
status_t TEST_mac_empty(crypto_context_t *c, te_crypto_algo_t algo, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	static uint8_t mac[64];	// largest mac value
	uint32_t mac_size = 0U;
	static uint8_t key[16];
	static te_args_key_data_t akey;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, algo);

	se_util_mem_set((uint8_t *)&akey, 0U, sizeof_u32(akey));
	se_util_mem_set(mac, 0U, sizeof_u32(mac));
	se_util_mem_set(key, 0U, sizeof_u32(key));

	/* Preset fields, do everythin in one syscall */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = algo;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	// Set the MAC key
	if (algo == TE_ALG_CMAC_AES) {
		akey.k_key_type = KEY_TYPE_AES;
		akey.k_flags = KEY_FLAG_PLAIN;
		akey.k_keyslot = AES_KEYSLOT;
		akey.k_byte_size = sizeof_u32(key);
		se_util_mem_move(akey.k_aes_key, key, akey.k_byte_size);

		LOG_INFO("Hint: use engine 0x%x (%s) for CMAC AES\n", eid, eid_name(eid));
		arg.ca_init.engine_hint = eid;
	} else {
		akey.k_key_type = KEY_TYPE_HMAC;
		akey.k_flags = KEY_FLAG_PLAIN;
		akey.k_byte_size = sizeof_u32(key);
		se_util_mem_move(akey.k_hmac_key, key, akey.k_byte_size);

		LOG_INFO("Hint: use engine 0x%x (%s) for HMAC digest\n", eid, eid_name(eid));
		arg.ca_init.engine_hint = eid;
	}

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

	static const uint8_t hm_sha1[] = {
		0xfb, 0xdb, 0x1d, 0x1b, 0x18, 0xaa, 0x6c, 0x08,
		0x32, 0x4b, 0x7d, 0x64, 0xb7, 0x1f, 0xb7, 0x63,
		0x70, 0x69, 0x0e, 0x1d,
	};

	static const uint8_t hm_sha224[] = {
		0x5C, 0xE1, 0x4F, 0x72, 0x89, 0x46, 0x62, 0x21,
		0x3E, 0x27, 0x48, 0xD2, 0xA6, 0xBA, 0x23, 0x4B,
		0x74, 0x26, 0x39, 0x10, 0xCE, 0xDD, 0xE2, 0xF5,
		0xA9, 0x27, 0x15, 0x24,
	};

	static const uint8_t hm_sha256[] = {
		0xb6, 0x13, 0x67, 0x9a, 0x08, 0x14, 0xd9, 0xec,
		0x77, 0x2f, 0x95, 0xd7, 0x78, 0xc3, 0x5f, 0xc5,
		0xff, 0x16, 0x97, 0xc4, 0x93, 0x71, 0x56, 0x53,
		0xc6, 0xc7, 0x12, 0x14, 0x42, 0x92, 0xc5, 0xad,
	};

	static const uint8_t hm_sha384[] = {
		0x6c, 0x1f, 0x2e, 0xe9, 0x38, 0xfa, 0xd2, 0xe2,
		0x4b, 0xd9, 0x12, 0x98, 0x47, 0x43, 0x82, 0xca,
		0x21, 0x8c, 0x75, 0xdb, 0x3d, 0x83, 0xe1, 0x14,
		0xb3, 0xd4, 0x36, 0x77, 0x76, 0xd1, 0x4d, 0x35,
		0x51, 0x28, 0x9e, 0x75, 0xe8, 0x20, 0x9c, 0xd4,
		0xb7, 0x92, 0x30, 0x28, 0x40, 0x23, 0x4a, 0xdc,
	};

	static const uint8_t hm_sha512[] = {
		0xb9, 0x36, 0xce, 0xe8, 0x6c, 0x9f, 0x87, 0xaa,
		0x5d, 0x3c, 0x6f, 0x2e, 0x84, 0xcb, 0x5a, 0x42,
		0x39, 0xa5, 0xfe, 0x50, 0x48, 0x0a, 0x6e, 0xc6,
		0x6b, 0x70, 0xab, 0x5b, 0x1f, 0x4a, 0xc6, 0x73,
		0x0c, 0x6c, 0x51, 0x54, 0x21, 0xb3, 0x27, 0xec,
		0x1d, 0x69, 0x40, 0x2e, 0x53, 0xdf, 0xb4, 0x9a,
		0xd7, 0x38, 0x1e, 0xb0, 0x67, 0xb3, 0x38, 0xfd,
		0x7b, 0x0c, 0xb2, 0x22, 0x47, 0x22, 0x5d, 0x47,
	};

	static const uint8_t hm_md5[] = {
		0x74, 0xe6, 0xf7, 0x29, 0x8a, 0x9c, 0x2d, 0x16,
		0x89, 0x35, 0xf5, 0x8c, 0x00, 0x1b, 0xad, 0x88,
	};

	static const uint8_t cmac_128[] = {
		0x43, 0x87, 0xc1, 0x4b, 0x46, 0xef, 0x7e, 0x17,
		0x6d, 0xce, 0xef, 0xa8, 0x62, 0xd7, 0x2f, 0xf9,
	};

	const uint8_t *correct = NULL;

	// XXX Support HMACs with NIST partial digests 512/224 and 512/256??
	switch (algo) {
	case TE_ALG_HMAC_MD5:
		correct = hm_md5;
		TRAP_ASSERT(mac_size == 16);
		break;
	case TE_ALG_HMAC_SHA1:
		correct = hm_sha1;
		TRAP_ASSERT(mac_size == 20);
		break;
	case TE_ALG_HMAC_SHA224:
		correct = hm_sha224;
		TRAP_ASSERT(mac_size == 28);
		break;
	case TE_ALG_HMAC_SHA256:
		correct = hm_sha256;
		TRAP_ASSERT(mac_size == 32);
		break;
	case TE_ALG_HMAC_SHA384:
		correct = hm_sha384;
		TRAP_ASSERT(mac_size == 48);
		break;
	case TE_ALG_HMAC_SHA512:
		correct = hm_sha512;
		TRAP_ASSERT(mac_size == 64);
		break;

	case TE_ALG_CMAC_AES:
		correct = cmac_128;
		TRAP_ASSERT(mac_size == 16);
		break;

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		LOG_INFO("Correct result not defined for MAC 0x%x\n", algo);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (NULL != correct) {
		VERIFY_ARRAY_VALUE(mac, correct, mac_size);
	} else {
		LOG_ERROR("%s: algo 0x%x result not verified\n", __func__, algo);
	}

fail:
	CRYPTO_CONTEXT_RESET(c);
	se_util_mem_set((uint8_t *)&akey, 0U, sizeof_u32(akey));
	return ret;
}
#endif /* HAVE_HMAC_SHA || HAVE_CMAC_AES */

#if HAVE_SHA1 && HAVE_HMAC_SHA
static status_t TEST_mac_simple(crypto_context_t *c, te_crypto_algo_t algo, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };
	static te_args_key_data_t akey;

	static uint8_t mac[64];

	static uint8_t correct_hmac_sha1_10[10] = {
		0xc7, 0x3d, 0x3c, 0xf2, 0xbd, 0x6c, 0x5c, 0x9d, 0xcb, 0x91,
	};

	static uint8_t data[] = {
		0x07, 0x35, 0x5a, 0xc8, 0x18, 0xce, 0x6b, 0x46, 0xd3, 0x41, 0x63, 0xae, 0xec, 0x45, 0xab, 0x17, 0x2d, 0x4b, 0x85, 0x0b, 0x0d, 0xbb, 0x42, 0xe6, 0x83, 0x81, 0xb6, 0x7f, 0x1c, 0xc8, 0xe9, 0x0a, 0x4c, 0x05, 0x0f, 0x3d, 0x01, 0x38, 0xba, 0xb2, 0x7e, 0x6f, 0x4f, 0x8d, 0x67, 0x8b, 0xb6, 0x5e, 0x18, 0x46, 0x56, 0x49, 0x3b, 0x75, 0x41, 0x64, 0x9a, 0x8b, 0xab, 0x60, 0x31, 0x5f, 0xa1, 0x6c, 0x88, 0x2f, 0xf8, 0x56, 0x40, 0xe4, 0x83, 0xf3, 0xeb, 0x97, 0x89, 0xc2, 0x21, 0x55, 0x75, 0xcc, 0xd0, 0x1f, 0xd0, 0xce, 0xd3, 0x35, 0x6d, 0x9a, 0xc6, 0x95, 0xe3, 0xbb, 0x19, 0xbe, 0x40, 0x58, 0x64, 0xb9, 0xfc, 0x5b, 0xfa, 0x5a, 0x2c, 0xd1, 0xc1, 0xc4, 0xf8, 0x94, 0x41, 0x2b, 0x4f, 0x28, 0xfa, 0xde, 0xda, 0xe4, 0xfb, 0x84, 0x2e, 0x52, 0xb0, 0xa5, 0x45, 0xd8, 0xfc, 0x6d, 0x2f, 0x97,
	};

	static uint8_t key[] = {
		0x15, 0x0d, 0x3a, 0xa3, 0x09, 0xa3, 0x66, 0x9a, 0xf9, 0x9a, 0x70, 0xf2, 0xce, 0xc5, 0x2d, 0x3d, 0xa1, 0x6b, 0x1c, 0x13, 0x7f, 0xf7, 0x46, 0x62, 0x69, 0xf2, 0x68, 0x05, 0x9f, 0x2f, 0x54, 0x98, 0x1f, 0x45, 0x95, 0x8b, 0x68, 0x42, 0x52, 0x76, 0x83, 0x9e, 0x75, 0xac, 0x44, 0x6e, 0x0b, 0x13, 0xce, 0xda, 0xee, 0x33, 0x55, 0xd1, 0xa2, 0x8c, 0x28, 0xfc, 0x7e, 0x2d, 0xee, 0xf0, 0x0c, 0x82, 0x2f, 0xa7, 0xb2, 0x6e, 0x17, 0x31,
	};

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, algo);

	se_util_mem_set(mac, 0U, sizeof_u32(mac));
	se_util_mem_set((uint8_t *)&akey, 0U, sizeof_u32(akey));

	/* Setup key material */
	akey.k_key_type = KEY_TYPE_HMAC;
	akey.k_flags = KEY_FLAG_PLAIN;
	akey.k_byte_size = sizeof_u32(key);

	se_util_mem_move(akey.k_hmac_key, key, akey.k_byte_size);

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = algo;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg.ca_init.engine_hint = eid;

	arg.ca_set_key.kdata = &akey;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(mac);
	arg.ca_data.dst = mac;

	ret = CRYPTO_OPERATION(c, &arg);

	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	CCC_ERROR_CHECK(ret);

	DUMP_HEX("MAC value:", mac, arg.ca_data.dst_size);

	// Verifies only the first 10 bytes (some kind of truncated HMAC_SHA1) */
	VERIFY_ARRAY_VALUE(mac, correct_hmac_sha1_10, sizeof_u32(correct_hmac_sha1_10));
fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}
#endif /* HAVE_SHA1 && HAVE_HMAC_SHA */


#if HAVE_HMAC_SHA
status_t TEST_hmac(crypto_context_t *c, te_crypto_algo_t mac, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	unsigned char data[16] = "abcdefghijklmnop";
	uint8_t key[16];
	static uint8_t key128[128];
	uint8_t dst[64];
	static te_args_key_data_t akey;
	uint32_t mac_size = 0U;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, mac);

	se_util_mem_set((uint8_t *)&akey, 0U, sizeof_u32(akey));

	// Ascii zero key (not binary zero key)
	se_util_mem_set(key, 0x30U, sizeof_u32(key));
	se_util_mem_set(key128, 0x30U, sizeof_u32(key128));
	se_util_mem_set(dst, 0U, sizeof_u32(dst));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = mac;
	arg.ca_opcode   = op_INIT;

	LOG_INFO("Hint: use engine 0x%x (%s) for HMAC digest\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	/* the arg.ca_handle is filled by the syscall (allocate) with
	 * a "per-TA" unique value to identify the context
	 * for the following crypto calls.
	 */
	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	LOG_INFO("MAC(init): ALGO: 0x%x, ret %d, attached handle: %u\n",
		 mac, ret, arg.ca_handle);
	CCC_ERROR_CHECK(ret);

	/* Set the HMAC key fields at runtime */

	akey.k_key_type = KEY_TYPE_HMAC;
	akey.k_flags = KEY_FLAG_PLAIN;
	if (mac == TE_ALG_HMAC_SHA256) {
		akey.k_byte_size = sizeof_u32(key128);
		se_util_mem_move(akey.k_hmac_key, key128, akey.k_byte_size);
	} else {
		akey.k_byte_size = sizeof_u32(key);
		se_util_mem_move(akey.k_hmac_key, key, akey.k_byte_size);
	}

	arg.ca_set_key.kdata = &akey;
	arg.ca_opcode = TE_OP_SET_KEY;

	ret = CRYPTO_OPERATION(c, &arg);
	se_util_mem_set((uint8_t *)&akey, 0U, sizeof_u32(akey));

	LOG_INFO("HMAC(set_key) handle %u: ret %d\n",
		 arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	arg.ca_opcode = op_FINAL_REL;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("HMAC(dofinal/release) handle %u: ret %d\n",
		 arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	mac_size = arg.ca_data.dst_size;

	/* verify that the result is correct */

	static const uint8_t hm_sha1[] = {
		0x4c, 0x45, 0x05, 0x7c, 0xec, 0x11, 0x05, 0x60,
		0xad, 0x05, 0x4f, 0x66, 0xaf, 0xf5, 0xc3, 0xa5,
		0xf2, 0x06, 0xeb, 0xe6,
	};

	static const uint8_t hm_sha224[] = {
		0xf6, 0xf7, 0x92, 0x17, 0x65, 0x8c, 0xe6, 0xa8,
		0xa6, 0xc9, 0x59, 0x75, 0x6f, 0xac, 0x14, 0xb9,
		0xf6, 0xc6, 0x51, 0x70, 0x4b, 0xd4, 0x70, 0xe9,
		0x09, 0x9b, 0xc8, 0x6d,
	};

	static const uint8_t hm_sha256_k128[] = {
		0x32, 0x47, 0x62, 0x5a, 0x8b, 0xa3, 0xd4, 0xe7,
		0x5e, 0xd5, 0x9c, 0xbf, 0xdc, 0x4a, 0xf5, 0xd0,
		0x63, 0xe0, 0x51, 0x4a, 0xa7, 0x49, 0x4f, 0x12,
		0x5f, 0x3c, 0x7b, 0xa6, 0xb3, 0xf7, 0x7f, 0x50,
	};

	static const uint8_t hm_sha384[] = {
		0x36, 0x56, 0xef, 0xe4, 0xb1, 0x8e, 0x53, 0x43,
		0xa6, 0xb4, 0x8e, 0x97, 0xd7, 0xc7, 0xd9, 0x32,
		0x7f, 0xc7, 0x1e, 0xee, 0xd8, 0x3c, 0xd4, 0x6a,
		0xb1, 0xe4, 0x55, 0xd8, 0x28, 0xa8, 0x6c, 0x63,
		0xf8, 0x84, 0x7f, 0xef, 0x1a, 0x17, 0x76, 0xc7,
		0xca, 0x9d, 0x72, 0xe6, 0x6d, 0x04, 0x45, 0x15,
	};

	static const uint8_t hm_sha512[] = {
		0x29, 0x06, 0x6a, 0xa4, 0x30, 0x93, 0x97, 0x31,
		0x9c, 0xe3, 0x9c, 0x50, 0xb8, 0xda, 0xd1, 0xaf,
		0xab, 0x6e, 0xf9, 0x4f, 0xde, 0xe6, 0x0b, 0x6a,
		0xfe, 0x99, 0x74, 0x0e, 0xec, 0x08, 0x7b, 0xe5,
		0x79, 0x48, 0x32, 0x03, 0x5b, 0x81, 0x93, 0xac,
		0x26, 0xe3, 0xda, 0x4c, 0xac, 0xdd, 0x37, 0x1d,
		0x83, 0x27, 0x74, 0xd9, 0x90, 0x2a, 0x6d, 0x17,
		0x26, 0x33, 0x25, 0x6f, 0x13, 0x96, 0xd8, 0x0e,
	};

	static const uint8_t hm_md5[] = {
		0x25, 0x69, 0xd1, 0xc8, 0x4c, 0xaa, 0x52, 0x54,
		0x5a, 0xb4, 0xa2, 0x75, 0x5c, 0x01, 0xed, 0xf9,
	};

	static const uint8_t cmac_128[] = {
		0xd9, 0x09, 0xb9, 0xaa, 0x4a, 0xab, 0xbd, 0x60,
		0x1c, 0x4c, 0xb7, 0xd4, 0x8a, 0x09, 0xcf, 0x3c,
	};

	const uint8_t *correct = NULL;

	// XXX Support HMACs with NIST partial digests 512/224 and 512/256??
	switch (mac) {
	case TE_ALG_HMAC_MD5:
		correct = hm_md5;
		TRAP_ASSERT(mac_size == 16);
		break;
	case TE_ALG_HMAC_SHA1:
		correct = hm_sha1;
		TRAP_ASSERT(mac_size == 20);
		break;
	case TE_ALG_HMAC_SHA224:
		correct = hm_sha224;
		TRAP_ASSERT(mac_size == 28);
		break;
	case TE_ALG_HMAC_SHA256:
		correct = hm_sha256_k128;
		TRAP_ASSERT(mac_size == 32);
		break;
	case TE_ALG_HMAC_SHA384:
		correct = hm_sha384;
		TRAP_ASSERT(mac_size == 48);
		break;
	case TE_ALG_HMAC_SHA512:
		correct = hm_sha512;
		TRAP_ASSERT(mac_size == 64);
		break;

	case TE_ALG_CMAC_AES:
		correct = cmac_128;
		TRAP_ASSERT(mac_size == 16);
		break;

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		LOG_INFO("Correct result not defined for MAC 0x%x\n", mac);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (NULL != correct) {
		VERIFY_ARRAY_VALUE(dst, correct, mac_size);
	} else {
		LOG_ERROR("%s: algo 0x%x result not verified\n", __func__, mac);
	}

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

static status_t hmac_tester(crypto_context_t *c, te_crypto_algo_t mac, engine_id_t eid,
			    const uint8_t *data, uint32_t dlen, const uint8_t *key, uint32_t klen,
			    const uint8_t *correct, uint32_t correct_len)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };
	uint8_t dst[64];
	uint32_t mac_size = 0U;
	// use file local work_akey

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, mac);

	se_util_mem_set((uint8_t *)&work_akey, 0U, sizeof_u32(work_akey));
	se_util_mem_set(dst, 0U, sizeof_u32(dst));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = mac;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	LOG_INFO("Hint: use engine 0x%x (%s) for HMAC digest\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	/* Set the HMAC key fields at runtime */

	work_akey.k_key_type = KEY_TYPE_HMAC;
	work_akey.k_flags = KEY_FLAG_PLAIN;
	work_akey.k_byte_size = klen;
	se_util_mem_move(work_akey.k_hmac_key, key, klen);

	arg.ca_set_key.kdata = &work_akey;

	arg.ca_data.src_size = dlen;
	arg.ca_data.src = (const uint8_t *)data;
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("HMAC: ret %d\n", ret);
	CCC_ERROR_CHECK(ret);

	mac_size = arg.ca_data.dst_size;

	/* verify that the result is correct */

	if (NULL != correct) {
		if (mac_size != correct_len) {
			CCC_ERROR_WITH_ECODE(ERR_NOT_VALID, LOG_ERROR("HMAC size not correct: %u != %u\n",
								      mac_size, correct_len));
		}
		VERIFY_ARRAY_VALUE(dst, correct, mac_size);
	} else {
		LOG_ERROR("%s: algo 0x%x result not verified\n", __func__, mac);
	}
fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}

static status_t hmac_rfc4231_test_case_1(crypto_context_t *crypto_ctx, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	uint32_t tid = 0U;

	LOG_ALWAYS("rfc4231 HMAC-SHA test case 1\n");

	const uint8_t key[] = {
		0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
		0x0b, 0x0b, 0x0b, 0x0b,
	};

	// "Hi There"
	const uint8_t data[] = {
		0x48, 0x69, 0x20, 0x54, 0x68, 0x65, 0x72, 0x65,
	};

#if HAVE_SHA224
	tid++;
	{
		const uint8_t r224[] = {
			0x89, 0x6f, 0xb1, 0x12, 0x8a, 0xbb, 0xdf, 0x19, 0x68, 0x32, 0x10, 0x7c, 0xd4, 0x9d, 0xf3, 0x3f,
			0x47, 0xb4, 0xb1, 0x16, 0x99, 0x12, 0xba, 0x4f, 0x53, 0x68, 0x4b, 0x22,
		};

		TEST_ERRCHK(hmac_tester, eid, TE_ALG_HMAC_SHA224,
			    data, sizeof_u32(data), key, sizeof_u32(key), r224, sizeof_u32(r224));
	}
#endif

	tid++;
	{
		const uint8_t r256[] = {
			0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0x0b, 0xf1, 0x2b,
			0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83, 0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7,
		};

		TEST_ERRCHK(hmac_tester, eid, TE_ALG_HMAC_SHA256,
			    data, sizeof_u32(data), key, sizeof_u32(key), r256, sizeof_u32(r256));
	}

	tid++;
	{
		const uint8_t r384[] = {
			0xaf, 0xd0, 0x39, 0x44, 0xd8, 0x48, 0x95, 0x62, 0x6b, 0x08, 0x25, 0xf4, 0xab, 0x46, 0x90, 0x7f,
			0x15, 0xf9, 0xda, 0xdb, 0xe4, 0x10, 0x1e, 0xc6, 0x82, 0xaa, 0x03, 0x4c, 0x7c, 0xeb, 0xc5, 0x9c,
			0xfa, 0xea, 0x9e, 0xa9, 0x07, 0x6e, 0xde, 0x7f, 0x4a, 0xf1, 0x52, 0xe8, 0xb2, 0xfa, 0x9c, 0xb6,
		};

		TEST_ERRCHK(hmac_tester, eid, TE_ALG_HMAC_SHA384,
			    data, sizeof_u32(data), key, sizeof_u32(key), r384, sizeof_u32(r384));
	}

	tid++;
	{
		const uint8_t r512[] = {
			0x87, 0xaa, 0x7c, 0xde, 0xa5, 0xef, 0x61, 0x9d, 0x4f, 0xf0, 0xb4, 0x24, 0x1a, 0x1d, 0x6c, 0xb0,
			0x23, 0x79, 0xf4, 0xe2, 0xce, 0x4e, 0xc2, 0x78, 0x7a, 0xd0, 0xb3, 0x05, 0x45, 0xe1, 0x7c, 0xde,
			0xda, 0xa8, 0x33, 0xb7, 0xd6, 0xb8, 0xa7, 0x02, 0x03, 0x8b, 0x27, 0x4e, 0xae, 0xa3, 0xf4, 0xe4,
			0xbe, 0x9d, 0x91, 0x4e, 0xeb, 0x61, 0xf1, 0x70, 0x2e, 0x69, 0x6c, 0x20, 0x3a, 0x12, 0x68, 0x54,
		};

		TEST_ERRCHK(hmac_tester, eid, TE_ALG_HMAC_SHA512,
			    data, sizeof_u32(data), key, sizeof_u32(key), r512, sizeof_u32(r512));
	}

	LOG_ALWAYS("rfc4231 HMAC-SHA test case 1 PASSED\n");
fail:
	if (NO_ERROR != ret) {
		LOG_ERROR("rfc4231 HMAC-SHA test case 1 FAILED, id=%u\n", tid);
	}
	return ret;
}

static status_t TEST_hmac_rfc4231(crypto_context_t *c, te_crypto_algo_t mac, engine_id_t eid)
{
	status_t ret = NO_ERROR;

	(void)mac;

	ret = hmac_rfc4231_test_case_1(c, eid);
	CCC_ERROR_CHECK(ret);

fail:
	if (NO_ERROR != ret) {
		LOG_ERROR("rfc4231 HMAC-SHA tests FAILED\n");
	}

	return ret;
}

static status_t TEST_hmac_with_update_byte(crypto_context_t *c, te_crypto_algo_t mac, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	unsigned char data[16] = "abcdefghijklmnop";
	uint8_t key[16];
	uint8_t dst[64];
	static te_args_key_data_t akey;
	uint32_t mac_size = 0U;
	uint32_t i;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC, mac);

	se_util_mem_set((uint8_t *)&akey, 0U, sizeof_u32(akey));

	// Ascii zero key (not binary zero key)
	se_util_mem_set(key, 0x30U, sizeof_u32(key));
	se_util_mem_set(dst, 0U, sizeof_u32(dst));

	/* Preset fields */
	arg.ca_alg_mode = TE_ALG_MODE_MAC;
	arg.ca_algo     = mac;
	arg.ca_opcode   = op_INIT;

	LOG_INFO("Hint: use engine 0x%x (%s) for HMAC digest\n", eid, eid_name(eid));
	arg.ca_init.engine_hint = eid;

	/* the arg.ca_handle is filled by the syscall (allocate) with
	 * a "per-TA" unique value to identify the context
	 * for the following crypto calls.
	 */
	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));
	LOG_INFO("MAC(init): ALGO: 0x%x, ret %d, attached handle: %u\n",
		 mac, ret, arg.ca_handle);
	CCC_ERROR_CHECK(ret);

	/* Set the HMAC key fields at runtime */

	akey.k_key_type = KEY_TYPE_HMAC;
	akey.k_flags = KEY_FLAG_PLAIN;
	akey.k_byte_size = sizeof_u32(key);
	se_util_mem_move(akey.k_hmac_key, key, akey.k_byte_size);

	arg.ca_set_key.kdata = &akey;
	arg.ca_opcode = TE_OP_SET_KEY;

	ret = CRYPTO_OPERATION(c, &arg);
	se_util_mem_set((uint8_t *)&akey, 0U, sizeof_u32(akey));

	LOG_INFO("HMAC(set_key) handle %u: ret %d\n",
		 arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	for (i = 0; i < sizeof_u32(data)-1; i++)
	{
		arg.ca_opcode = TE_OP_UPDATE;

		arg.ca_data.src_size = 1;
		arg.ca_data.src = &data[i];

		ret = CRYPTO_OPERATION(c, &arg);
		LOG_INFO("HMAC(update) handle %u: ret %d\n",
			 arg.ca_handle,ret);
		CCC_ERROR_CHECK(ret);
	}

	arg.ca_opcode = op_FINAL_REL;

	arg.ca_data.src_size = 1;
	arg.ca_data.src = &data[i];
	arg.ca_data.dst_size = sizeof_u32(dst);
	arg.ca_data.dst = dst;

	ret = CRYPTO_OPERATION(c, &arg);
	LOG_INFO("HMAC(dofinal/release) handle %u: ret %d\n",
		 arg.ca_handle,ret);
	CCC_ERROR_CHECK(ret);

	mac_size = arg.ca_data.dst_size;

	/* verify that the result is correct */

	static const uint8_t hm_sha1[] = {
		0x4c, 0x45, 0x05, 0x7c, 0xec, 0x11, 0x05, 0x60,
		0xad, 0x05, 0x4f, 0x66, 0xaf, 0xf5, 0xc3, 0xa5,
		0xf2, 0x06, 0xeb, 0xe6,
	};

	static const uint8_t hm_sha224[] = {
		0xf6, 0xf7, 0x92, 0x17, 0x65, 0x8c, 0xe6, 0xa8,
		0xa6, 0xc9, 0x59, 0x75, 0x6f, 0xac, 0x14, 0xb9,
		0xf6, 0xc6, 0x51, 0x70, 0x4b, 0xd4, 0x70, 0xe9,
		0x09, 0x9b, 0xc8, 0x6d,
	};

	static const uint8_t hm_sha256[] = {
		0xbe, 0x83, 0x88, 0xe2, 0x05, 0xea, 0xf0, 0x57,
		0x80, 0xff, 0x3b, 0x21, 0x50, 0x05, 0xbc, 0x5f,
		0x1d, 0xee, 0x1d, 0x66, 0x70, 0x59, 0x28, 0xe5,
		0x67, 0x1e, 0x58, 0xfd, 0xf4, 0x74, 0xb2, 0xc5,
	};

	static const uint8_t hm_sha384[] = {
		0x36, 0x56, 0xef, 0xe4, 0xb1, 0x8e, 0x53, 0x43,
		0xa6, 0xb4, 0x8e, 0x97, 0xd7, 0xc7, 0xd9, 0x32,
		0x7f, 0xc7, 0x1e, 0xee, 0xd8, 0x3c, 0xd4, 0x6a,
		0xb1, 0xe4, 0x55, 0xd8, 0x28, 0xa8, 0x6c, 0x63,
		0xf8, 0x84, 0x7f, 0xef, 0x1a, 0x17, 0x76, 0xc7,
		0xca, 0x9d, 0x72, 0xe6, 0x6d, 0x04, 0x45, 0x15,
	};

	static const uint8_t hm_sha512[] = {
		0x29, 0x06, 0x6a, 0xa4, 0x30, 0x93, 0x97, 0x31,
		0x9c, 0xe3, 0x9c, 0x50, 0xb8, 0xda, 0xd1, 0xaf,
		0xab, 0x6e, 0xf9, 0x4f, 0xde, 0xe6, 0x0b, 0x6a,
		0xfe, 0x99, 0x74, 0x0e, 0xec, 0x08, 0x7b, 0xe5,
		0x79, 0x48, 0x32, 0x03, 0x5b, 0x81, 0x93, 0xac,
		0x26, 0xe3, 0xda, 0x4c, 0xac, 0xdd, 0x37, 0x1d,
		0x83, 0x27, 0x74, 0xd9, 0x90, 0x2a, 0x6d, 0x17,
		0x26, 0x33, 0x25, 0x6f, 0x13, 0x96, 0xd8, 0x0e,
	};

	static const uint8_t hm_md5[] = {
		0x25, 0x69, 0xd1, 0xc8, 0x4c, 0xaa, 0x52, 0x54,
		0x5a, 0xb4, 0xa2, 0x75, 0x5c, 0x01, 0xed, 0xf9,
	};

	const uint8_t cmac_128[] = {
		0xd9, 0x09, 0xb9, 0xaa, 0x4a, 0xab, 0xbd, 0x60,
		0x1c, 0x4c, 0xb7, 0xd4, 0x8a, 0x09, 0xcf, 0x3c,
	};

	const uint8_t *correct = NULL;

	// XXX Support HMACs with NIST partial digests 512/224 and 512/256??
	switch (mac) {
	case TE_ALG_HMAC_MD5:
		correct = hm_md5;
		TRAP_ASSERT(mac_size == 16);
		break;
	case TE_ALG_HMAC_SHA1:
		correct = hm_sha1;
		TRAP_ASSERT(mac_size == 20);
		break;
	case TE_ALG_HMAC_SHA224:
		correct = hm_sha224;
		TRAP_ASSERT(mac_size == 28);
		break;
	case TE_ALG_HMAC_SHA256:
		correct = hm_sha256;
		TRAP_ASSERT(mac_size == 32);
		break;
	case TE_ALG_HMAC_SHA384:
		correct = hm_sha384;
		TRAP_ASSERT(mac_size == 48);
		break;
	case TE_ALG_HMAC_SHA512:
		correct = hm_sha512;
		TRAP_ASSERT(mac_size == 64);
		break;

	case TE_ALG_CMAC_AES:
		correct = cmac_128;
		TRAP_ASSERT(mac_size == 16);
		break;

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		LOG_INFO("Correct result not defined for MAC 0x%x\n", mac);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (NULL != correct) {
		VERIFY_ARRAY_VALUE(dst, correct, mac_size);
	} else {
		LOG_ERROR("%s: algo 0x%x result not verified\n", __func__, mac);
	}

fail:
	CRYPTO_CONTEXT_RESET(c);
	return ret;
}
#endif // HAVE_HMAC_SHA

#if HAVE_HMAC_SHA || HAVE_CMAC_AES
static status_t TEST_mac_verify(crypto_context_t *c, te_crypto_algo_t mac, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	te_crypto_args_t arg = { .ca_handle = 0U, };

	unsigned char data[16] = "abcdefghijklmnop";
	uint8_t key[16];
	static uint8_t key128[128];
	uint8_t dst[64];
	static te_args_key_data_t akey;
	uint32_t src_mac_size = 0U;
	const uint8_t *src_mac = NULL;

	CRYPTO_CONTEXT_SETUP(c, TE_ALG_MODE_MAC_VERIFY, mac);

	se_util_mem_set((uint8_t *)&akey, 0U, sizeof_u32(akey));

	// Ascii zero key (not binary zero key)
	se_util_mem_set(key, 0x30U, sizeof_u32(key));
	se_util_mem_set(key128, 0x30U, sizeof_u32(key128));
	se_util_mem_set(dst, 0U, sizeof_u32(dst));

	/* Set the HMAC key fields at runtime */

	akey.k_key_type = KEY_TYPE_HMAC;
	akey.k_flags = KEY_FLAG_PLAIN;

	if (mac == TE_ALG_HMAC_SHA256) {
		akey.k_byte_size = sizeof_u32(key128);
		se_util_mem_move(akey.k_hmac_key, key128, akey.k_byte_size);
	} else {
		akey.k_byte_size = sizeof_u32(key);
		se_util_mem_move(akey.k_hmac_key, key, akey.k_byte_size);
	}

	/* Arg fields */
	arg.ca_alg_mode = TE_ALG_MODE_MAC_VERIFY;
	arg.ca_algo     = mac;

	arg.ca_init.engine_hint = eid;
	LOG_INFO("Hint: use engine 0x%x (%s) for MAC VERIFY\n", eid, eid_name(eid));

	arg.ca_set_key.kdata = &akey;
	arg.ca_opcode   = TE_OP_COMBINED_OPERATION;

	arg.ca_data.src_size = sizeof_u32(data);
	arg.ca_data.src = data;

	static const uint8_t hm_sha1[] = {
		0x4c, 0x45, 0x05, 0x7c, 0xec, 0x11, 0x05, 0x60,
		0xad, 0x05, 0x4f, 0x66, 0xaf, 0xf5, 0xc3, 0xa5,
		0xf2, 0x06, 0xeb, 0xe6,
	};

	static const uint8_t hm_sha224[] = {
		0xf6, 0xf7, 0x92, 0x17, 0x65, 0x8c, 0xe6, 0xa8,
		0xa6, 0xc9, 0x59, 0x75, 0x6f, 0xac, 0x14, 0xb9,
		0xf6, 0xc6, 0x51, 0x70, 0x4b, 0xd4, 0x70, 0xe9,
		0x09, 0x9b, 0xc8, 0x6d,
	};

	static const uint8_t hm_sha256_k128[] = {
		0x32, 0x47, 0x62, 0x5a, 0x8b, 0xa3, 0xd4, 0xe7,
		0x5e, 0xd5, 0x9c, 0xbf, 0xdc, 0x4a, 0xf5, 0xd0,
		0x63, 0xe0, 0x51, 0x4a, 0xa7, 0x49, 0x4f, 0x12,
		0x5f, 0x3c, 0x7b, 0xa6, 0xb3, 0xf7, 0x7f, 0x50,
	};

	static const uint8_t hm_sha384[] = {
		0x36, 0x56, 0xef, 0xe4, 0xb1, 0x8e, 0x53, 0x43,
		0xa6, 0xb4, 0x8e, 0x97, 0xd7, 0xc7, 0xd9, 0x32,
		0x7f, 0xc7, 0x1e, 0xee, 0xd8, 0x3c, 0xd4, 0x6a,
		0xb1, 0xe4, 0x55, 0xd8, 0x28, 0xa8, 0x6c, 0x63,
		0xf8, 0x84, 0x7f, 0xef, 0x1a, 0x17, 0x76, 0xc7,
		0xca, 0x9d, 0x72, 0xe6, 0x6d, 0x04, 0x45, 0x15,
	};

	static const uint8_t hm_sha512[] = {
		0x29, 0x06, 0x6a, 0xa4, 0x30, 0x93, 0x97, 0x31,
		0x9c, 0xe3, 0x9c, 0x50, 0xb8, 0xda, 0xd1, 0xaf,
		0xab, 0x6e, 0xf9, 0x4f, 0xde, 0xe6, 0x0b, 0x6a,
		0xfe, 0x99, 0x74, 0x0e, 0xec, 0x08, 0x7b, 0xe5,
		0x79, 0x48, 0x32, 0x03, 0x5b, 0x81, 0x93, 0xac,
		0x26, 0xe3, 0xda, 0x4c, 0xac, 0xdd, 0x37, 0x1d,
		0x83, 0x27, 0x74, 0xd9, 0x90, 0x2a, 0x6d, 0x17,
		0x26, 0x33, 0x25, 0x6f, 0x13, 0x96, 0xd8, 0x0e,
	};

	static const uint8_t hm_md5[] = {
		0x25, 0x69, 0xd1, 0xc8, 0x4c, 0xaa, 0x52, 0x54,
		0x5a, 0xb4, 0xa2, 0x75, 0x5c, 0x01, 0xed, 0xf9,
	};

	static const uint8_t cmac_128[] = {
		0xd9, 0x09, 0xb9, 0xaa, 0x4a, 0xab, 0xbd, 0x60,
		0x1c, 0x4c, 0xb7, 0xd4, 0x8a, 0x09, 0xcf, 0x3c,
	};

	// XXX Could support MACs with NIST partial digests 512/224 and 512/256
	switch (mac) {
	case TE_ALG_HMAC_MD5:
		src_mac = hm_md5;
		src_mac_size = 16;
		break;
	case TE_ALG_HMAC_SHA1:
		src_mac = hm_sha1;
		src_mac_size = 20;
		break;
	case TE_ALG_HMAC_SHA224:
		src_mac = hm_sha224;
		src_mac_size = 28;
		break;
	case TE_ALG_HMAC_SHA256:
		src_mac = hm_sha256_k128;
		src_mac_size = 32;
		break;
	case TE_ALG_HMAC_SHA384:
		src_mac = hm_sha384;
		src_mac_size = 48;
		break;
	case TE_ALG_HMAC_SHA512:
		src_mac = hm_sha512;
		src_mac_size = 64;
		break;
	case TE_ALG_CMAC_AES:
		akey.k_key_type = KEY_TYPE_AES;

		src_mac = cmac_128;
		src_mac_size = 16;
		break;
	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		LOG_INFO("Correct result not defined for MAC 0x%x\n", mac);
		break;
	}
	CCC_ERROR_CHECK(ret);

	arg.ca_data.src_mac_size = src_mac_size;
	arg.ca_data.src_mac	 = src_mac;

	/* Do a MAC_VERIFY operation with the given MAC
	 */
	ret = CRYPTO_OPERATION(c, &arg);
	CCC_ERROR_CHECK(ret);

	LOG_INFO("Operation used engine 0x%x (%s)\n",
		 arg.ca_init.engine_hint,
		 eid_name(arg.ca_init.engine_hint));

	LOG_ALWAYS("MAC VERIFY SUCCESS\n");
fail:
	if (NO_ERROR != ret) {
		LOG_ERROR("MAC VERIFY failed %d\n", ret);
	}

	CRYPTO_CONTEXT_RESET(c);
	return ret;
}
#endif /* HAVE_HMAC_SHA || HAVE_CMAC_AES */

#if HAVE_CMAC_AES
status_t run_mac_test_cases(crypto_context_t *crypto_ctx)
{
	status_t ret = NO_ERROR;

	(void)crypto_ctx;

	TEST_ERRCHK(TEST_cmac_with_update, CCC_ENGINE_SE0_AES0, TE_ALG_CMAC_AES);
	TEST_ERRCHK(TEST_cmac_with_update, CCC_ENGINE_SE0_AES1, TE_ALG_CMAC_AES);

	TEST_ERRCHK(TEST_cmac_preset_key_64_bytes, CCC_ENGINE_SE0_AES1, TE_ALG_CMAC_AES);
	TEST_ERRCHK(TEST_cmac_preset_key_64_bytes, CCC_ENGINE_SE0_AES0, TE_ALG_CMAC_AES);
	TEST_ERRCHK(TEST_cmac_one_pass, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);
	TEST_ERRCHK(TEST_cmac_with_update_byte, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);
	TEST_ERRCHK(TEST_cmac, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);

	TEST_ERRCHK(TEST_mac_empty, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);
#ifdef TEST_SEC_SAFE_REQ_VERIFY
	/* CCC security requirement to verify HW reg. cleared after crypto operations  */
	TEST_ERRCHK(TEST_cmac_aes_clr_hw_regs_verify, CCC_ENGINE_SE0_AES0, TE_ALG_CMAC_AES);
	TEST_ERRCHK(TEST_cmac_aes_clr_hw_regs_verify, CCC_ENGINE_SE0_AES1, TE_ALG_CMAC_AES);
#endif

	/* MAC VERIFY tests with CMAC-AES
	 */
	TEST_ERRCHK(TEST_mac_verify, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);
fail:
	return ret;
}

#if HAVE_CMAC_DST_KEYTABLE
status_t run_cmac_to_keytable_test_cases(crypto_context_t *crypto_ctx)
{
	status_t ret = NO_ERROR;

#if CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T18X)
	TEST_EXPECT_RET(TEST_cmac_key_derive128, ERR_NOT_ALLOWED, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);
	TEST_EXPECT_RET(TEST_cmac_key_derive256, ERR_NOT_ALLOWED, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);

	TEST_EXPECT_RET(TEST_cmac_key_derive128_long, ERR_NOT_ALLOWED, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);
	TEST_EXPECT_RET(TEST_cmac_key_derive256_long, ERR_NOT_ALLOWED, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);

	TEST_EXPECT_RET(TEST_cmac_key_derive128_preset_key, ERR_NOT_ALLOWED, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);
	TEST_EXPECT_RET(TEST_cmac_key_derive256_preset_key, ERR_NOT_ALLOWED, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);
#else
	TEST_ERRCHK(TEST_cmac_key_derive128, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);
	TEST_ERRCHK(TEST_cmac_key_derive256, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);

	TEST_ERRCHK(TEST_cmac_key_derive128_long, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);
	TEST_ERRCHK(TEST_cmac_key_derive256_long, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);

	TEST_ERRCHK(TEST_cmac_key_derive128_preset_key, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);
	TEST_ERRCHK(TEST_cmac_key_derive256_preset_key, CCC_ENGINE_ANY, TE_ALG_CMAC_AES);
#endif

fail:
	return ret;
}
#endif /* HAVE_CMAC_DST_KEYTABLE */

#endif /* HAVE_CMAC_AES */

#if HAVE_HMAC_SHA
status_t run_hmac_test_cases(crypto_context_t *crypto_ctx)
{
	status_t ret = NO_ERROR;

	TEST_ERRCHK(TEST_hmac_rfc4231, CCC_ENGINE_ANY, TE_ALG_NONE);
#if HAVE_SHA1
	TEST_ERRCHK(TEST_mac_empty, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA1);
	TEST_ERRCHK(TEST_mac_simple, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA1);
	TEST_ERRCHK(TEST_hmac, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA1);
#endif

#if HAVE_SHA224
	TEST_ERRCHK(TEST_mac_empty, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA224);
	TEST_ERRCHK(TEST_hmac, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA224);
#endif

	TEST_ERRCHK(TEST_mac_empty, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA256);

#ifndef HAVE_VDK_ERROR_SKIP
	TEST_ERRCHK(TEST_mac_empty, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA384);
#endif

	TEST_ERRCHK(TEST_mac_empty, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA512);

	TEST_ERRCHK(TEST_hmac, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA256);
	TEST_ERRCHK(TEST_hmac, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA384);
	TEST_ERRCHK(TEST_hmac, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA512);

	TEST_ERRCHK(TEST_hmac_with_update_byte, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA512);

	/* MAC VERIFY tests with HMAC
	 */
#if HAVE_SHA1
	TEST_ERRCHK(TEST_mac_verify, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA1);
#endif
#if HAVE_SHA224
	TEST_ERRCHK(TEST_mac_verify, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA224);
#endif
	TEST_ERRCHK(TEST_mac_verify, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA256);
	TEST_ERRCHK(TEST_mac_verify, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA384);
	TEST_ERRCHK(TEST_mac_verify, CCC_ENGINE_ANY, TE_ALG_HMAC_SHA512);
fail:
	return ret;
}
#endif /* HAVE_HMAC_SHA */

#endif /* KERNEL_TEST_MODE */

