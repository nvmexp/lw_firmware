/*
 * Copyright (c) 2015-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */
#include <crypto_system_config.h>

#include <ccc_lwrm_drf.h>
#include <arse0.h>

#include <crypto_ops.h>
#include <tegra_se_gen.h>

#if HAVE_SOC_FEATURE_KAC == 0
#error "HAVE_SOC_FEATURE_KAC not set"
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if HAVE_SE_GENRND

/* Use one of the SE AES0 engine to generate DRNG random numbers
 */
static status_t se_config_drng(const struct se_engine_rng_context *econtext)
{
	status_t ret = NO_ERROR;
	uint32_t se_config_reg = 0U;

	LTRACEF("entry\n");

	/* CONFIG register */

	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, ENC_ALG, RNG,
					   se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DEC_ALG, NOP,
					   se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DST, MEMORY,
					   se_config_reg);

	tegra_engine_set_reg(econtext->engine, SE0_AES0_CONFIG_0, se_config_reg);

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t get_aligned_genrnd_arg_check(uint32_t bytes, const uint8_t *vbuf)
{
	status_t ret = NO_ERROR;

	if ((NULL == vbuf) || (0U == bytes)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* HW size limit for output
	 */
	if (bytes > (16U * 1024U * 1024U)) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG);
	}

	/* Both the number of bytes and the buffer address need to be CACHE_LINE
	 * aligned for this function.
	 */
	if (!(IS_DMA_ALIGNED(&vbuf[0]) && IS_DMA_ALIGNED(bytes))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

fail:
	return ret;
}

#if SE_RD_DEBUG
static status_t rd_debug_rand_output_check(const uint8_t *vbuf, uint32_t bytes)
{
	status_t ret = NO_ERROR;
	uint32_t zeros = 0U;
	uint32_t c_zeros = 0U;
	uint32_t i = 0U;

	/* Trivial check for GENRND output quality */
	for(i = 0U; i < bytes; i++) {
		if (0U == vbuf[i]) {
			zeros++;
			c_zeros++;
		} else {
			c_zeros = 0U;
		}

#define C_ZERO_LIMIT 6U
		if (c_zeros > C_ZERO_LIMIT) {
			LOG_HEXBUF("GENRND failed data (c_zero)", vbuf, bytes);
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
					     CCC_ERROR_MESSAGE("Random data has more than %u conselwtive zero bytes (position: %u)\n",
							       C_ZERO_LIMIT, i));
		}
	}

	if (zeros > (bytes / 2U)) {
		/* poor quality entropy is bad, better fail it */
		LOG_HEXBUF("GENRND failed data (zerocount)", vbuf, bytes);
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("simple random quality validation failed: %u zeros in %u bytes\n",
						       zeros, bytes));
	}

fail:
	return ret;
}
#endif /* SE_RD_DEBUG */

static status_t genrnd_bytes_op_run(
	const struct se_engine_rng_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	SE_ERR_CHECK(econtext->engine, "GENRND START");

	{
#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
		uint32_t gen_op_perf_usec = GET_USEC_TIMESTAMP;
#endif

		/**
		 * Issue START command in SE_OPERATION.OP
		 */
		ret = tegra_start_se0_operation_locked(econtext->engine, 0U);
		CCC_ERROR_CHECK(ret);

		ret = se_wait_engine_free(econtext->engine);
		CCC_ERROR_CHECK(ret);

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
		if (0U != gen_op_perf_usec) {
			show_se_gen_op_entry_perf("GENRND",
						  get_elapsed_usec(gen_op_perf_usec));
		}
#endif
	}

	SE_ERR_CHECK(econtext->engine, "GENRND DONE");

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t get_aligned_genrnd_bytes(
	const struct se_engine_rng_context *econtext,
	uint32_t bytes, uint8_t *vbuf)
{
	status_t ret = NO_ERROR;
	PHYS_ADDR_T paddr = PADDR_NULL;
	uint32_t se_config_reg = 0U;
	uint32_t num_blocks = 0U;

	LTRACEF("entry\n");

	DEBUG_ASSERT_HW_MUTEX;

	ret = get_aligned_genrnd_arg_check(bytes, vbuf);
	CCC_ERROR_CHECK(ret);

	DEBUG_ASSERT((CACHE_LINE % SE_AES_BLOCK_LENGTH) == 0U);

	num_blocks = bytes / SE_AES_BLOCK_LENGTH;

	if (0U == num_blocks) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = se_config_drng(econtext);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to config SE GENRND: %d\n",
					  ret));

	paddr = DOMAIN_VADDR_TO_PADDR(econtext->domain, vbuf);
	SE_PHYS_RANGE_CHECK(paddr, bytes);

	/* Just flush for the DST here; it will get ilwalidated only
	 * once after the GENRND operation completes. This makes a
	 * difference in a write-through cache system.
	 */
	SE_CACHE_FLUSH((VIRT_ADDR_T)vbuf, DMA_ALIGN_SIZE(bytes));

	/* Program output address (low 32 bits) */
	tegra_engine_set_reg(econtext->engine, SE0_AES0_OUT_ADDR_0,
			     se_util_lsw_u64((uint64_t)paddr));

	/* Program output address (hi bits) */
	se_config_reg = 0U;
	se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, OUT_ADDR_HI, SZ, bytes,
					   se_config_reg);

	/* And the high 8 bits of the 40 bit address */
	se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, OUT_ADDR_HI, MSB,
					   SE_GET_32_UPPER_PHYSADDR_BITS(paddr),
					   se_config_reg);

	tegra_engine_set_reg(econtext->engine, SE0_AES0_OUT_ADDR_HI_0,
			     se_config_reg);

	/* Last block is num_blocks - 1 */
	tegra_engine_set_reg(econtext->engine, SE0_AES0_CRYPTO_LAST_BLOCK_0,
			     num_blocks - 1U);

#if HAVE_SE_APERTURE
	ret = se_set_aperture(PADDR_NULL, 0U, paddr, bytes);
	CCC_ERROR_CHECK(ret);
#endif

	ret = genrnd_bytes_op_run(econtext);
	CCC_ERROR_CHECK(ret);

	SE_CACHE_ILWALIDATE((VIRT_ADDR_T)vbuf, bytes);

	LTRACEF("GENRND %u aligned bytes to %p\n", bytes, vbuf);

#if SE_RD_DEBUG
	ret = rd_debug_rand_output_check(vbuf, bytes);
	CCC_ERROR_CHECK(ret);
#endif /* SE_RD_DEBUG */

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t engine_genrnd_generate_check_args(
	const struct se_data_params *input_params,
	const struct se_engine_rng_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == input_params)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == input_params->dst) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (0U == input_params->output_size) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* WIP:
 *
 * Commit adding use of econtext->dma_buffer was unfortunately merged too early.
 * Use a workaround to prevent compile error on systems
 * enabling HAVE_SE_GENRND until fixed.
 */
#ifndef RNG_DMA_BUFFER_BSIZE
#define RNG_DMA_BUFFER_BSIZE CACHE_LINE
#define GENRAND_STACK_BUFFER
#endif

static status_t se_genrnd_generate_unaligned(
	const struct se_engine_rng_context *econtext,
	uint8_t *vaddr_param,
	uint32_t bytes_param)
{
	status_t ret = NO_ERROR;
	uint32_t bytes = bytes_param;
	CCC_LOOP_VAR;
	uint8_t *vaddr = vaddr_param;

	/* For GENRND unaligned case => use the engine dma_buffer
	 * which is set up by the calling chain. From CCC api it is
	 * set up in crypto_random.c init call.
	 *
	 * Any caller and especially engine layer direct callers need
	 * to set up this buffer or the call returns ERR_ILWALID_ARGS.
	 */
	LTRACEF("entry\n");

#ifdef GENRAND_STACK_BUFFER
	uint8_t DMA_ALIGN_DATA rbuf[RNG_DMA_BUFFER_BSIZE];
#else
	uint8_t *rbuf = econtext->dma_buffer;

	if (NULL == rbuf) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Buffer for GENRND unaligned DMA not set up\n"));
	}
#endif

	if (!IS_DMA_ALIGNED(rbuf)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Work buffer not DMA aligned\n"));
	}

	CCC_LOOP_BEGIN {
		ret = get_aligned_genrnd_bytes(econtext,
					       RNG_DMA_BUFFER_BSIZE,
					       rbuf);
		CCC_ERROR_END_LOOP(ret,
				   LTRACEF("Failed to fill GENRND econtext DMA buffer\n"));

		if (bytes <= RNG_DMA_BUFFER_BSIZE) {
			se_util_mem_move(vaddr, &rbuf[0], bytes);
			CCC_LOOP_STOP;
		}

		se_util_mem_move(vaddr, &rbuf[0], RNG_DMA_BUFFER_BSIZE);

		bytes -= RNG_DMA_BUFFER_BSIZE;
		vaddr  = &vaddr[RNG_DMA_BUFFER_BSIZE];
	} CCC_LOOP_END;

	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to generate GENRND: %d (unaligned)\n", ret));

	LTRACEF("GENRND %u bytes to %p (unaligned request)\n",
		bytes_param, vaddr_param);
fail:
#ifdef GENRAND_STACK_BUFFER
	se_util_mem_set(&rbuf[0], 0U, RNG_DMA_BUFFER_BSIZE);
#else
	if (NULL != rbuf) {
		se_util_mem_set(&rbuf[0], 0U, RNG_DMA_BUFFER_BSIZE);
	}
#endif

	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t engine_genrnd_generate_locked(
	struct se_data_params *input_params,
	const struct se_engine_rng_context *econtext)
{
	status_t ret = NO_ERROR;
	uint32_t bytes = 0U;
	uint8_t *vaddr = NULL;

	LTRACEF("entry\n");

	DEBUG_ASSERT_HW_MUTEX;

	ret = engine_genrnd_generate_check_args(input_params, econtext);
	CCC_ERROR_CHECK(ret);

	bytes = input_params->output_size;
	vaddr = input_params->dst;

	if (!(IS_DMA_ALIGNED(vaddr) && IS_DMA_ALIGNED(bytes))) {
		ret = se_genrnd_generate_unaligned(econtext, vaddr, bytes);
		CCC_ERROR_CHECK(ret);
	} else {
		ret = get_aligned_genrnd_bytes(econtext, bytes, vaddr);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("Failed to generate SE RNG: %d (aligned)\n", ret));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_CLOSE_FUNCTIONS
void engine_rng0_close_locked(void)
{
	// Nothing lwrrently
}
#endif
#endif /* HAVE_SE_GENRND */
