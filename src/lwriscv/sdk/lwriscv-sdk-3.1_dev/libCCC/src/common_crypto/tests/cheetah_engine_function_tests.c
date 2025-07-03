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

/* Tests for the CCC SE engine API layer.
 *
 * These calls can only be called directly from kernel level code (i.e. if you
 * use CCC as a library in your address space you can also call these).
 */
#include <crypto_system_config.h>

#include <crypto_md_proc.h>
#include <crypto_cipher_proc.h>

#include <tests/setest_kernel_tests.h>

#if MODULE_TRACE
#define LOCAL_TRACE 1
#else
#define LOCAL_TRACE 0
#endif

/* define in your makefile */
#ifndef HAVE_ENGINE_API_TESTS
#define HAVE_ENGINE_API_TESTS 0
#endif

#if HAVE_ENGINE_API_TESTS

status_t run_engine_api_tests(void);

/* Prototype for the SHA engine function you can call to callwlate SHA-256
 * of contiguous input data in a single call.
 */
static status_t tegra_engine_sha256(struct context_mem_s *cmem,
				    const uint8_t *src, uint32_t size,
				    uint8_t *dst)
{
	status_t ret = NO_ERROR;
	struct se_engine_sha_context econtext = { .cmem = NULL };
	struct se_data_params input;

#if TEGRA_MEASURE_MCP
	measure_mcp_init();
#endif

	LTRACEF("entry\n");

	if (size >= MAX_SHA_ENGINE_CHUNK_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG,
				     CCC_ERROR_MESSAGE("Engine can not process chunk of size %u\n", size));
	}

	ret = sha_engine_context_static_init_cm(TE_CRYPTO_DOMAIN_KERNEL,
						CCC_ENGINE_SE0_SHA,
						TE_ALG_SHA256,
						&econtext,
						cmem);
	CCC_ERROR_CHECK(ret);

	econtext.is_last = true;

	input.src = src;
	input.input_size = size;
	input.dst = dst;
	input.output_size = 32U;

	HW_MUTEX_TAKE_ENGINE(econtext.engine);
	ret = engine_sha_process_block_locked(&input, &econtext);
	HW_MUTEX_RELEASE_ENGINE(econtext.engine);

	CCC_ERROR_CHECK(ret,
			LTRACEF("sha digest failed: %d\n", ret));
fail:
	LTRACEF("exit: %d\n", ret);

#if TEGRA_MEASURE_MCP
	measure_mcp_results(true);
#endif

	return ret;
}

#if HAVE_SE_ASYNC_SHA

/*
 * Async SHA engine API use case. (Client api is probably simpler, but
 * this bypasses that and the process level apis)
 *
 * Feel free to split the following function to two and then call the
 * latter part when you need the SHA result.
 *
 * Between these two parts you can do what you want except you can not
 * NOT USE SE engine for something else because the mutex is now
 * locked for you.  (if you try to use SE your code will block waiting
 * for mutex which in a single threaded system means a deadlock).
 *
 */
static status_t tegra_engine_async_sha256(struct context_mem_s *cmem,
					  const uint8_t *src, uint32_t size,
					  uint8_t *dst)
{
	status_t ret = NO_ERROR;

	/* These two objects must exist until end of async finish
	 * (like src and dst must exist until sha digest is callwlated).
	 */
	struct se_engine_sha_context econtext = { .cmem = NULL };
	async_sha_ctx_t ac = { .started = false };
	bool mutex_held = false;

#if TEGRA_MEASURE_MCP
	measure_mcp_init();
#endif

	LTRACEF("entry\n");

	if ((NULL == src) || (NULL == dst)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (size >= MAX_SHA_ENGINE_CHUNK_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG,
				     CCC_ERROR_MESSAGE("Engine can not process chunk of size %u\n", size));
	}

	ret = sha_engine_context_static_init_cm(TE_CRYPTO_DOMAIN_KERNEL,
						CCC_ENGINE_SE0_SHA,
						TE_ALG_SHA256,
						&econtext,
						cmem);
	CCC_ERROR_CHECK(ret);

	econtext.is_last = true;

	ac.econtext = &econtext;
	ac.magic = SHA_ASYNC_MAGIC;

	ac.input.src = src;
	ac.input.input_size = size;
	ac.input.dst = dst;
	ac.input.output_size = 32U;

	/* Take mutex.
	 *
	 * Note that this also starts the mutex watchdog so you have
	 * about 1.6 seconds (@601 MHz) to either call the optional
	 * status check status (resets watchdog again) or release mutex
	 * before the watchdog timeouts.
	 */
	HW_MUTEX_TAKE_ENGINE(econtext.engine);
	mutex_held = true;

	/* Starts the SHA engine to process the data and returns
	 */
	ret = engine_sha_locked_async_start(&ac);
	CCC_ERROR_CHECK(ret);

	LTRACEF("async start returned\n");

	/* Here you can do almost what you want except you can not NOT
	 * USE SE engine because the mutex is now locked for you.  (if
	 * you try to use SE for something through CCC your code will
	 * block waiting for mutex which in a single threaded system
	 * means a deadlock).
	 *
	 * You could e.g. split this function in two and call the
	 * latter part to "finish" the SHA callwlations. That call
	 * will block until the SHA has completed.
	 */

	/* Could split the function in half e.g. here ;-) */

	{
		/* You can now also call the non-blocking check if the SHA
		 * operation has already completed (anytime after the above
		 * start call):
		 *
		 */
		bool is_idle = false;

		LTRACEF("async poll called\n");
		ret = engine_sha_locked_async_done(&ac, &is_idle);
		CCC_ERROR_CHECK(ret);

		if (BOOL_IS_TRUE(is_idle)) {
			LTRACEF("SHA poll: operation completed\n");
		} else {
			LTRACEF("SHA poll: operation ongoing\n");
		}
	}

	LTRACEF("async finish called\n");

	/* Call this when you want to get the SHA digest result to dst buffer.
	 * Waits for completion, gets result, release mutex.
	 *
	 * This call blocks until SHA digest is ready; so you do not need to
	 * use the optional check function above.
	 */
	ret = engine_sha_locked_async_finish(&ac);
	CCC_ERROR_CHECK(ret);

	LTRACEF("async finish returned\n");

fail:
	if (BOOL_IS_TRUE(mutex_held)) {
		HW_MUTEX_RELEASE_ENGINE(econtext.engine);
	}

	LTRACEF("exit: %d\n", ret);

#if TEGRA_MEASURE_MCP
	measure_mcp_results(true);
#endif

	/* Clear for completeness */
	se_util_mem_set((uint8_t *)&ac, 0, sizeof_u32(async_sha_ctx_t));
	return ret;
}

/* Async SHA256 digest test for client specified size chunks for arbitrary size data
 *
 * The CHUNK SIZE must be a multiple of SHA256 BLOCK SIZE (verified). This is an
 * engine limitation when intermediate results are callwlated in SE engine tasks.
 */
#define HAVE_ERROR_CHECKS 0
#define SHA256_BLOCK_SIZE (512U / 8U)
#define SHA256_SIZE 32U

/* You can either use sync api or async api for any chunk you pass
 * to CCC SHA engine code. If this is set, the sync api is used for the last chunk.
 */
#define USE_SYNC_SHA_FOR_LAST_CHUNK 0

static status_t tegra_se_async_sha256_chunk_digest_locked(async_sha_ctx_t *ac,
							  uint32_t chunk_size,
							  uint32_t total_size)
{
	status_t ret = NO_ERROR;
	uint32_t index = 0U;

	uint8_t *zero_data = NULL;
	uint8_t digest[ SHA256_SIZE ];
	uint32_t chunks = 0U;
	uint32_t remain = 0U;
	uint32_t loop_limit = 0U;

	if ((NULL == ac) || (NULL == ac->econtext) ||
	    (0U == total_size) || (0U == chunk_size) ||
	    (chunk_size % SHA256_BLOCK_SIZE != 0U)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	chunks = total_size / chunk_size;
	remain = total_size % chunk_size;
	loop_limit = chunks;

	/* This is just cache line aligned sample data passed to SHA DMA engine,
	 * use your own data buffers instead.
	 */
	zero_data = CMTAG_MEM_GET_BUFFER(NULL,
	    CMTAG_ALIGNED_BUFFER, CACHE_LINE, chunk_size);
	if (NULL == zero_data) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

#if TEGRA_MEASURE_MCP
	measure_mcp_init();
#endif

	/* Constant zero buffer input as every SHA input chunk.
	 *
	 * Any chunk size from SHA256_BLOCK_SIZE to 16 MB is ok, but
	 * all except the last chunk must be an even multiple of
	 * SHA256_BLOCK_SIZE bytes.
	 *
	 * For simplicity and lack of memory, just use one page (4096) of zero bytes
	 * to each async sha call. It makes more sense to pass larger blocks to it
	 * in your code.
	 */
	ac->input.src	   = zero_data;
	ac->input.input_size = chunk_size;
	ac->input.dst      = NULL;
	ac->input.output_size = 0U;

	/* Loop here passing data to async functions.
	 * Do not pass the last block in the loop, adjust the
	 * counter so that even if the total_size is multiple
	 * of SHA256 block size the last block is skipped.
	 */
	if (0U == remain) {
		if (loop_limit > 0U) {
			loop_limit--;
		}

		remain = chunk_size;
	}

	/* loop through the chunks of zeroes.
	 */
	for (index = 0U; index < loop_limit; index++) {

#if HAVE_ERROR_CHECKS
		/* Engine restriction: intermediate chunks MUST BE SHA
		 * digest block size multiples. In this case it always
		 * is (<block size> * 1) => no need to check.
		 */
		if ((ac->input.input_size % ac->econtext->block_size) != 0U) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
		}
#endif

		/* A pair of these async calls completes each SHA chunk digest.
		 */
		ret = engine_sha_locked_async_start(ac);
		CCC_ERROR_CHECK(ret);

		LTRACEF("Client code can be run after this point\n");

		/* SHA engine is callwlating the SHA value of a data chunk here.
		 * From SE engine point of view this is an "SE TASK" which may
		 * be followed by another SE task which adds data to the previous
		 * digest.
		 *
		 * You can do other things here, but do not try to use SE engine
		 * to avoid deadlock.
		 */

		LTRACEF("CCC code entered\n");
		/* Get the intermediate or final result out of engine.
		 * This call blocks if the engine is not ready.
		 *
		 * To optionally check if the engine is done, use the
		 * non-blocking check function before calling finish().
		 * See the previous example for that.
		 */
		ret = engine_sha_locked_async_finish(ac);
		CCC_ERROR_CHECK(ret);

#if 0 && !TEGRA_MEASURE_MCP && SE_RD_DEBUG
		/* Do not dump this if measuring times; this is the internal intermediate SHA digest
		 */
		DUMP_HEX("SHA256 value", ac->econtext->tmphash, SHA256_SIZE);
#endif
	}

	/* Call the SHA256 with the final input data for this SHA value.
	 */
	ac->econtext->is_last = true;

	/* Number of bytes for the final SHA chunk (this is never zero here)
	 * and does not need to be size aligned in any way.
	 */
	ac->input.input_size = remain;

	/* For the last call => pass the digest output address
	 * and size.
	 */
	ac->input.dst = digest;
	ac->input.output_size = sizeof_u32(digest);

#if USE_SYNC_SHA_FOR_LAST_CHUNK
	/* You can also use the SE SHA sync api for any chunk if you
	 * have nothing special to do between the start / finish calls.
	 */
	ret = engine_sha_process_block_locked(&ac->input, ac->econtext);
	CCC_ERROR_CHECK(ret);
#else
	/* A pair of these async calls completes each SHA chunk digest.
	 */
	ret = engine_sha_locked_async_start(ac);
	CCC_ERROR_CHECK(ret);

	LTRACEF("Client code can be run after this point\n");

	/* Run client code here */

	LTRACEF("CCC code entered\n");
	ret = engine_sha_locked_async_finish(ac);
	CCC_ERROR_CHECK(ret);
#endif /* USE_SYNC_SHA_FOR_LAST_CHUNK */

fail:
	LTRACEF("exit: %d\n", ret);

#if TEGRA_MEASURE_MCP
	measure_mcp_results(true);
#endif

	if (NULL != zero_data) {
		CMTAG_MEM_RELEASE(NULL, CMTAG_ALIGNED_BUFFER, zero_data);
	}

#if SE_RD_DEBUG
	if (NO_ERROR == ret) {
		LOG_ALWAYS("SHA of %u zeros completed, %u chunks (unaligned bytes %u)\n",
			   total_size, chunks, remain);

		DUMP_HEX("SHA256 final value", digest, SHA256_SIZE);
	}
#endif
	return ret;
}

static status_t chunked_async_sha256(struct context_mem_s *cmem,
				     async_sha_ctx_t *ac,
				     uint32_t chunk_size,
				     uint32_t bytes)
{
	status_t ret = NO_ERROR;

	ret = sha_engine_context_static_init_cm(TE_CRYPTO_DOMAIN_KERNEL,
						CCC_ENGINE_SE0_SHA,
						TE_ALG_SHA256,
						ac->econtext,
						cmem);
	CCC_ERROR_CHECK(ret);

	HW_MUTEX_TAKE_ENGINE(ac->econtext->engine);
	ret = tegra_se_async_sha256_chunk_digest_locked(ac, chunk_size, bytes);
	HW_MUTEX_RELEASE_ENGINE(ac->econtext->engine);

	CCC_ERROR_CHECK(ret);

fail:
	return ret;
}

#endif /* HAVE_SE_ASYNC_SHA */

#if CCC_WITH_MODULAR && HAVE_GEN_MULTIPLY

#include <tegra_cdev.h>
#include <tegra_pka1_mod.h>

static status_t run_modular_multiply_engine_tests(struct context_mem_s *cmem)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;
	te_mod_params_t modpar = { .size = 0U };
	const engine_t *engine = NULL;
	bool match = false;
	uint32_t nbytes = 32U;
	CCC_LOOP_VAR;

	const uint8_t x[32U] = { 0x6B, 0x17, 0xD1, 0xF2, 0xE1, 0x2C, 0x42, 0x47,
				 0xF8, 0xBC, 0xE6, 0xE5, 0x63, 0xA4, 0x40, 0xF2,
				 0x77, 0x03, 0x7D, 0x81, 0x2D, 0xEB, 0x33, 0xA0,
				 0xF4, 0xA1, 0x39, 0x45, 0xD8, 0x98, 0xC2, 0x96, };
	const uint8_t y[32U] = {  0x4F, 0xE3, 0x42, 0xE2, 0xFE, 0x1A, 0x7F, 0x9B,
				  0x8E, 0xE7, 0xEB, 0x4A, 0x7C, 0x0F, 0x9E, 0x16,
				  0x2B, 0xCE, 0x33, 0x57, 0x6B, 0x31, 0x5E, 0xCE,
				  0xCB, 0xB6, 0x40, 0x68, 0x37, 0xBF, 0x51, 0xF5, };
	const uint8_t m[32U] = { 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
				 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				 0xBC, 0xE6, 0xFA, 0xAD, 0xA7, 0x17, 0x9E, 0x84,
				 0xF3, 0xB9, 0xCA, 0xC2, 0xFC, 0x63, 0x25, 0x51, };

	const uint8_t m_prime[32U] = {
		0x4F, 0xBC, 0x00, 0xEE, 0xAA, 0xC8, 0xD1, 0xCC,
		0xE4, 0xD2, 0x74, 0x7D, 0x08, 0x44, 0xC9, 0x48,
		0xF6, 0xC6, 0x88, 0xC5, 0xEC, 0x77, 0xFE, 0x50,
		0x1C, 0x28, 0xD6, 0xA9, 0x33, 0x66, 0xD0, 0x60,
	};
	const uint8_t r2[32U] = {
		0xA2, 0xEE, 0x79, 0xBE, 0x95, 0x4C, 0x24, 0x83,
		0xA6, 0x6F, 0xBD, 0x49, 0x9C, 0x79, 0x99, 0x46,
		0x59, 0xEC, 0x6B, 0x2B, 0x39, 0xB2, 0x45, 0x28,
		0x20, 0x56, 0xD9, 0xF3, 0x94, 0x2D, 0xE1, 0x66,
	};

	static uint8_t res1[64U];
	static uint8_t res2[32U];
	static uint8_t res3[32U];
	static uint8_t *res4 = &res1[32U];

	LOG_ALWAYS("[ Modular multiply engine tests ]\n");

	ret = ccc_select_engine(&engine, CCC_ENGINE_CLASS_EC, CCC_ENGINE_ANY);
	CCC_ERROR_CHECK(ret, LOG_ERROR("Could not select PKA EC engine\n"));

	res1[0] = 0x01U;
	res2[0] = 0x02U;
	res3[0] = 0x03U;
	res4[0] = 0x04U;

	/* Init for the modular operations.
	 * This object is not modified by the mod op calls.
	 */
	modpar.cmem = cmem;
	modpar.x = x;
	modpar.y = y;
	modpar.m = m;
	modpar.size = sizeof_u32(m);
	modpar.op_mode = PKA1_OP_MODE_ECC256;
	modpar.mod_flags = PKA1_MOD_FLAG_NONE;

	/* Double precision result, generic multiply */
	modpar.r = res1;

	HW_MUTEX_TAKE_ENGINE(engine);

	CCC_LOOP_BEGIN {
		/* res1(DP) = x * y
		 * res1 = res1 (mod m)
		 */
		ret = CCC_ENGINE_SWITCH_GEN_MULTIPLICATION(engine, &modpar);
		CCC_ERROR_END_LOOP(ret);

		modpar.x = res1;
		ret = CCC_ENGINE_SWITCH_MODULAR_BIT_REDUCTION_DP(engine, &modpar);
		CCC_ERROR_END_LOOP(ret);

		modpar.x = x;
		modpar.r = res2;	/* Single precision reduced result, gen multiply */

		/* res2 = (x * y) (mod m)
		 */
		ret = CCC_ENGINE_SWITCH_MODULAR_GEN_MULTIPLICATION(engine, &modpar);
		CCC_ERROR_END_LOOP(ret);

		modpar.x = x;
		modpar.r = res3;	/* modular multiply using mont values */

		/* res3 = x * y (mod m)
		 * Uses Montgomery values (not provided, callwlated by call)
		 */
		ret = CCC_ENGINE_SWITCH_MODULAR_MULTIPLICATION(engine, &modpar);
		CCC_ERROR_END_LOOP(ret);

		/* modular multiply with provided Little Endian mont values
		 */
		modpar.mont_m_prime = m_prime;
		modpar.mont_r2	    = r2;
		modpar.r	    = res4;

		modpar.mod_flags =
			(PKA1_MOD_FLAG_MONT_M_PRIME_LITTLE_ENDIAN |
			 PKA1_MOD_FLAG_MONT_R2_LITTLE_ENDIAN);

		/* res4 = x * y (mod m)
		 * Uses provided Montgomery values.
		 */
		ret = CCC_ENGINE_SWITCH_MODULAR_MULTIPLICATION(engine, &modpar);
		CCC_ERROR_END_LOOP(ret);

		CCC_LOOP_STOP;
	} CCC_LOOP_END;

	HW_MUTEX_RELEASE_ENGINE(engine);
	CCC_ERROR_CHECK(ret);

	/* Fail unless (res1 == res2 == res3)
	 */
	match = se_util_vec_match(res1, res2, nbytes, false, &rbad);
	if ((NO_ERROR != rbad) || !BOOL_IS_TRUE(match)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LOG_ERROR("EC multiply primitive test failure: (res1,res2)\n"));
	}

	match = se_util_vec_match(res2, res3, nbytes, false, &rbad);
	if ((NO_ERROR != rbad) || !BOOL_IS_TRUE(match)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LOG_ERROR("EC multiply primitive test failure: (res2,res3)\n"));
	}

	match = se_util_vec_match(res2, res4, nbytes, false, &rbad);
	if ((NO_ERROR != rbad) || !BOOL_IS_TRUE(match)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LOG_ERROR("EC multiply primitive test failure: (res2,res4)\n"));
	}

	LOG_ALWAYS("[ Modular multiply test OK ]\n");
fail:
	if (NO_ERROR != ret) {
		LOG_ERROR("[ ***ERROR: modular multiply engine test FAILED! (err 0x%x) ]\n",
			  ret);
	}
	return ret;
}
#endif /* CCC_WITH_MODULAR && HAVE_GEN_MULTIPLY */

static status_t run_sha_engine_api_tests(struct context_mem_s *cmem)
{
	status_t ret = NO_ERROR;

	/* These two objects must exist until the async finish
	 * with econtext->is_last == true has completed.
	 */
	struct se_engine_sha_context econtext = { .cmem = NULL };
	async_sha_ctx_t ac = { .started = false };

	ac.econtext = &econtext;
	ac.magic = SHA_ASYNC_MAGIC;

#if HAVE_SE_ASYNC_SHA
	/* First chunked SHA digest for 5000 zero bytes, page size chunks.
	 */
	ret = chunked_async_sha256(cmem, &ac, 4096U, 5000U);
	CCC_ERROR_CHECK(ret);

	/* Second SHA digest for 840K zero bytes, 256KB chunks.
	 */
	ret = chunked_async_sha256(cmem, &ac, (256U*1024U), (840U * 1024U));
	CCC_ERROR_CHECK(ret);
#endif /* HAVE_SE_ASYNC_SHA */

	{
		uint8_t buffer[ SHA256_SIZE ];

		se_util_mem_set(buffer, 0xFF, sizeof_u32(buffer));

		ret = tegra_engine_sha256(cmem, buffer, SHA256_SIZE, buffer);
		CCC_ERROR_CHECK(ret);

		DUMP_HEX("SHA256 engine sync digest (of 32 bytes)", buffer, SHA256_SIZE);

		se_util_mem_set(buffer, 0xFF, sizeof_u32(buffer));
		ret = tegra_engine_async_sha256(cmem, buffer,
						SHA256_SIZE, buffer);
		CCC_ERROR_CHECK(ret);

		DUMP_HEX("SHA256 engine async digest (of 32 bytes)", buffer, SHA256_SIZE);
	}

	if (CFALSE) {
	fail:
		LOG_ERROR("[ ***ERROR: SE SHA engine test case FAILED! (err 0x%x) ]\n",ret);
		// On failure must return error code
		if (NO_ERROR == ret) {
			ret = SE_ERROR(ERR_GENERIC);
		}
	}
	return ret;
}

#if HAVE_SE_ASYNC_AES

#define MAX_POLL_COUNT 1000000

static status_t aes_async_handle_one_chunk(async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;
	bool is_idle = true;
	bool mutex_held = false;
	uint32_t poll_count = 0U;
	bool done = false;

	HW_MUTEX_TAKE_ENGINE(ac->econtext->engine);

	mutex_held = true;

	/* Start AES ASYNC */
	ret = engine_aes_locked_async_start(ac);
	CCC_ERROR_CHECK(ret);

	do {
		poll_count++;

		/* POLL aes completion for the chunk
		 *
		 * Here you can do what you want except touch the SE engine.
		 * The SE HW mutex (if used) remains locked here!
		 *
		 * This is a non-blocking call to check if the operation
		 * has been completed.
		 */
		ret = engine_aes_locked_async_done(ac, &is_idle);
		if (NO_ERROR != ret) {
			break;
		}

		if (BOOL_IS_TRUE(is_idle)) {
			LTRACEF("AES poll: operation completed (%u)\n", poll_count);
			done = true;
		}
	} while (!BOOL_IS_TRUE(done) && (poll_count < MAX_POLL_COUNT));
	CCC_ERROR_CHECK(ret);

	if (MAX_POLL_COUNT == poll_count) {
		LTRACEF("Timeout on AES chunk polling, blocking until completed\n");
	}

	/* Finish this chunk, block util complete.
	 *
	 * You can call this also before operation has completed, so
	 * the above polling is optional. This blocks until the
	 * operation completes.
	 *
	 * The loop above drops here to wait for result on "timeout".
	 */
	ret = engine_aes_locked_async_finish(ac);
	CCC_ERROR_CHECK(ret);

fail:
	if (BOOL_IS_TRUE(mutex_held)) {
		HW_MUTEX_RELEASE_ENGINE(ac->econtext->engine);
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Must be multiple of 16 in engine api calls.
 * Recommended to be multiple of CACHE_LINE also.
 */
#define AES_CHUNK_SIZE 256U
#define MY_AES_KEYSLOT 3U

/* in-place cipher operation: overwrites buf.
 */
static status_t aes_all_chunks(async_aes_ctx_t *ac, uint8_t *buf, uint32_t bsize)
{
	status_t ret = NO_ERROR;
	uint32_t count = 0U;

	/* This would reject unaligned bytes. Could handle all data
	 * which is 16 byte aligned, but since we know all chunks are
	 * fixed size and total size is a multiple of that, this is ok.
	 */
	uint32_t chunks = bsize / AES_CHUNK_SIZE;
	uint8_t *ptr = buf;

	LTRACEF("entry\n");

	ac->input.input_size  = AES_CHUNK_SIZE;
	ac->input.output_size = AES_CHUNK_SIZE;

	ac->econtext->is_first = true;
	ac->econtext->is_last  = false;

	for (; count < (chunks - 1U); count++) {
		ac->input.src = ptr;
		ac->input.dst = ptr;

		ret = aes_async_handle_one_chunk(ac);
		if (NO_ERROR != ret) {
			break;
		}

		ptr = &ptr[AES_CHUNK_SIZE];
	}
	CCC_ERROR_CHECK(ret);

	/* Process last chunk, complete AES operation
	 */
	ac->econtext->is_last = true;

	ac->input.src = ptr;
	ac->input.dst = ptr;

	ret = aes_async_handle_one_chunk(ac);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t run_async_aes_engine_api_tests(struct context_mem_s *cmem)
{
	status_t ret = NO_ERROR;

	/* NOTE:
	 * These two objects must exist until the async finish
	 * with econtext->is_last == true has completed.
	 */
	struct se_engine_aes_context econtext = { .aes_flags = 0U, };
	async_aes_ctx_t ac = { .started = false };
	uint8_t *buffer = NULL;
	uint32_t total_size = (4U * AES_CHUNK_SIZE);

	const uint8_t aes_key[] = {
		0x0fU, 0x0eU, 0x0dU, 0x0lw, 0x0bU, 0x0aU, 0x09U, 0x08U,
		0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U
	};

	/* AES-CBC mode needs an IV value */
	uint8_t iv[] = {
		0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U,
		0x0fU, 0x0eU, 0x0dU, 0x0lw, 0x0bU, 0x0aU, 0x09U, 0x08U
	};

	LOG_ALWAYS("[ Async AES-CBC engine test start ]\n");

	/* This is both input and output buffer for the AES-CBC
	 * cipher. After ciphering we decipher the ciphered data
	 * and verify result is zero filled.
	 */
	buffer = CMTAG_MEM_GET_BUFFER(NULL,
				      CMTAG_ALIGNED_BUFFER,
				      CACHE_LINE, total_size);
	if (NULL == buffer) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	ac.use_preset_key       = true; /* Set the key before using it; leave it in keyslot */
	ac.leave_key_to_keyslot = true;

	/* Since the key is in keyslot no need to set it to:
	 * econtext.se_ks.ks_keydata = &aes_key[0];
	 */
	econtext.se_ks.ks_bitsize = 128U;
	econtext.se_ks.ks_slot    = MY_AES_KEYSLOT;

	/* If do not have enough context memory for tests, pass NULL.
	 * But the caller created one, so use that if required.
	 */
	ret = aes_engine_context_static_init_cm(TE_CRYPTO_DOMAIN_KERNEL,
						CCC_ENGINE_ANY,
						TE_ALG_AES_CBC_NOPAD,
						&econtext,
						cmem);
	CCC_ERROR_CHECK(ret);

	/* AES-CBC mode uses a caller specified IV value
	 */
	se_util_mem_move((uint8_t *)econtext.iv_stash, iv, sizeof_u32(iv));

	ac.econtext = &econtext;
	ac.magic = AES_ASYNC_MAGIC;

	/* Set the key if not already in the keyslot
	 *
	 * Set keyslot 3 for SE0 device.
	 */
	ret = se_set_device_aes_keyslot(SE_CDEV_SE0, MY_AES_KEYSLOT, &aes_key[0], 128U);
	CCC_ERROR_CHECK(ret, CCC_ERROR_MESSAGE("Failed to set AES key\n"));

	/* Encrypt input */
	econtext.is_encrypt = true;

	/* Encrypt the data in AES_CHUNK_SIZE blobs with async calls.
	 */
	ret = aes_all_chunks(&ac, buffer, total_size);
	CCC_ERROR_CHECK(ret);

	DUMP_HEX("Ciphered buffer", buffer, total_size);

	/* The stash got overwritten by encrypt calls.
	 */
	se_util_mem_move((uint8_t *)econtext.iv_stash, iv, sizeof_u32(iv));

	/* Engine context already initialized above.
	 * Just set it up for a new run, this time for decipher.
	 */
	econtext.is_encrypt = false;

	/* Decrypt the data in AES_CHUNK_SIZE blobs with async calls.
	 */
	ret = aes_all_chunks(&ac, buffer, total_size);
	CCC_ERROR_CHECK(ret);

	DUMP_HEX("Deciphered buffer", buffer, total_size);

	/* Verify decipher result is ZERO */
	{
		uint32_t inx = 0U;
		for (; inx < total_size; inx++) {
			if (0x00U != buffer[inx]) {
				CCC_ERROR_MESSAGE("Async AES-CBC cipher/decipher value differs, inx=%u\n", inx);
				CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
			}
		}
	}

	LOG_ALWAYS("[ Async AES-CBC engine test pass ]\n");
fail:
	(void)se_clear_device_aes_keyslot(SE_CDEV_SE0, MY_AES_KEYSLOT);

	if (NULL != buffer) {
		CMTAG_MEM_RELEASE(NULL, CMTAG_ALIGNED_BUFFER, buffer);
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_ASYNC_AES */

status_t run_engine_api_tests(void)
{
	status_t ret = NO_ERROR;
	struct context_mem_s *cmem = NULL;

#if HAVE_CONTEXT_MEMORY && (FORCE_CONTEXT_MEMORY == 0)
	struct context_mem_s cmem_obj = { 0U };
	uint8_t *mem = NULL;
	uint32_t offset = 0U;

	/* This needs to be a contiguous buffer aligned to 64 byte address for
	 * use in CMEM.
	 */
	mem = CMTAG_MEM_GET_BUFFER(NULL, CMTAG_ALIGNED_BUFFER, 64U, 4096U);
	if (NULL == mem) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	cmem = &cmem_obj;

#if CCC_WITH_CONTEXT_DMA_MEMORY
	/* Sample of how to set up a separate CMEM DMA memory into
	 * static memory buffer. Offset needs to be 64 byte aligned
	 * value.
	 *
	 * Digests do not use KEY buffer. Handle likewise with CCC_TYPE_KEY
	 * on algorithm with keys and separate key space.
	 */
	offset = (CMEM_DESCRIPTOR_SIZE + CCC_DMA_MEM_SHA_RESERVATION_SIZE);
	ret = cmem_from_buffer(cmem, CMEM_TYPE_DMA, &mem[0], offset);
	CCC_ERROR_CHECK(ret);
#endif

	/* Colwert a memory area to a CMEM and initialize the
	 * marked descriptor. This assigns the memory to the CMEM
	 * generic request types. All CMEM memory objects should be
	 * aligned to CMEM_DESCRIPTOR_SIZE (64 byte alignment) in
	 * case they are used for DMA objects.
	 */
	ret = cmem_from_buffer(cmem, CMEM_TYPE_GENERIC, &mem[offset], (4096U - offset));
	CCC_ERROR_CHECK(ret);
#endif /* HAVE_CONTEXT_MEMORY && (FORCE_CONTEXT_MEMORY == 0) */

#if HAVE_SE_ASYNC_AES
	ret = run_async_aes_engine_api_tests(cmem);
	CCC_ERROR_CHECK(ret);
#endif /* HAVE_SE_ASYNC_AES */

	ret = run_sha_engine_api_tests(cmem);
	CCC_ERROR_CHECK(ret);

#if CCC_WITH_MODULAR && HAVE_GEN_MULTIPLY
	ret = run_modular_multiply_engine_tests(cmem);
	CCC_ERROR_CHECK(ret);
#endif /* CCC_WITH_MODULAR && HAVE_GEN_MULTIPLY */

fail:
#if HAVE_CONTEXT_MEMORY && (FORCE_CONTEXT_MEMORY == 0)
	if (NULL != mem) {
		CMTAG_MEM_RELEASE(NULL, CMTAG_ALIGNED_BUFFER, mem);
	}
#endif
	return ret;
}
#endif /* HAVE_ENGINE_API_TESTS */
