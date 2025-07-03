/*
 * Copyright (c) 2017-2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/* Support CheetAh Security Engine Elliptic PKA core : random numbers.
 *
 * Support for PKA1 TRNG unit
 */
#include <crypto_system_config.h>

#if HAVE_PKA1_TRNG

#include <crypto_ops.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#define TRNG_WORDS 8U
#define TRNG_BYTES (TRNG_WORDS * BYTES_IN_WORD)

static void init_trng_selwre_autoreseed(const engine_t *engine, bool *init_p)
{
	uint32_t val = 0U;

	LTRACEF("entry\n");

	/* Enter secure and internal random auto-reseed mode */
	val = SE_PKA1_TRNG_SMODE_SELWRE(SE_ELP_ENABLE) |
		SE_PKA1_TRNG_SMODE_NONCE(SE_ELP_DISABLE);

	tegra_engine_set_reg(engine, SE_PKA1_TRNG_SMODE_OFFSET, val);

	/* Enable autoreseed */
	val = tegra_engine_get_reg(engine, SE_PKA1_CTRL_CONTROL_OFFSET);

	/* Setting this 1 is mandatory when SCC DPA enabled but we
	 * want TRNG to anyway autoreseed.
	 */
	val |= SE_PKA1_CTRL_CONTROL_AUTO_RESEED(SE_ELP_ENABLE);

	tegra_engine_set_reg(engine, SE_PKA1_CTRL_CONTROL_OFFSET, val);

	while (true) {
		val = tegra_engine_get_reg(engine, SE_PKA1_TRNG_STATUS_OFFSET);
		if ((val & SE_PKA1_TRNG_STATUS_SEEDED(SE_ELP_ENABLE)) != 0U) {
			break;
		}
	}

	*init_p = false;
	LTRACEF("exit\n");
}

status_t pka1_trng_setup_locked(const struct engine_s *engine)
{
	status_t ret = NO_ERROR;
	uint32_t val = 0U;
	bool init_trng = true;

	LTRACEF("entry\n");

	val = tegra_engine_get_reg(engine, SE_PKA1_TRNG_STATUS_OFFSET);

	/* Must be SECURE, not NONCE, SEEDED and seeded by "HOST" or "RESEED"
	 * If not => TRNG will be re-initialized.
	 *
	 * XXX Why "HOST" ??? See IAS on page 35 (allows both values 0 and 4) FIXME
	 */
	if ((val & SE_PKA1_TRNG_STATUS_NONCE(SE_ELP_ENABLE)) == 0U) {
		if ((val & SE_PKA1_TRNG_STATUS_SELWRE(SE_ELP_ENABLE)) != 0U) {
			if ((val & SE_PKA1_TRNG_STATUS_SEEDED(SE_ELP_ENABLE)) != 0U) {
				uint32_t last_seeded = (val >> SE_PKA1_TRNG_STATUS_LAST_RESEED_SHIFT) &
					SE_PKA1_TRNG_STATUS_LAST_RESEED_MASK;

				switch (last_seeded) {
				case TRNG_LAST_RESEED_HOST:
				case TRNG_LAST_RESEED_RESEED:
					init_trng = false;
					break;
				default:
					LTRACEF("TRNG not seeded properly: 0x%x\n", last_seeded);
					ret = SE_ERROR(ERR_BAD_STATE);
					break;
				}
				CCC_ERROR_CHECK(ret);
			}
		}
	}

	if (BOOL_IS_TRUE(init_trng)) {
		init_trng_selwre_autoreseed(engine, &init_trng);
	}

#if 0
	/* TODO: I guess could fail if TRNG does not get initialized */
	if (BOOL_IS_TRUE(init_trng)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}
#endif

#if TRNG_WORDS == 8U
	/* enable 256 bit TRNG value generation */
	val = SE_PKA1_TRNG_MODE_R256(SE_ELP_ENABLE);
#elif TRNG_WORDS == 4U
	/* disable 256 bit TRNG value generation */
	val = 0U;
#else
#error "PKA1 TRNG only supports 128 and 256 bit modes"
#endif
	tegra_engine_set_reg(engine, SE_PKA1_TRNG_MODE_OFFSET, val);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#ifndef MAX_TRNG_POLLS
#define MAX_TRNG_POLLS 0x00FFFFFFU
#endif

static status_t trng_copy_bytes(const engine_t *engine,
				uint32_t bytes,
				uint8_t *dst)
{
	status_t ret = NO_ERROR;
	uint32_t val = 0U;
	uint32_t words = bytes / BYTES_IN_WORD;
	uint32_t residue = bytes % BYTES_IN_WORD;
	uint32_t inx = 0U;
	uint8_t *p = &dst[0];

	LTRACEF("entry\n");

	for (; inx < words; inx++) {
		val = tegra_engine_get_reg(engine,
					   SE_PKA1_TRNG_RAND_BASE_OFFSET + (BYTES_IN_WORD * inx));
		LTRACEF("TRNG(%u) = 0x%x\n", inx, val);
		se_util_u32_to_buf(val, p, true);
		p = &p[BYTES_IN_WORD];
	}

	FI_BARRIER(u32, inx);
	if (inx != words) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	if (0U != residue) {
		val = tegra_engine_get_reg(engine,
					   SE_PKA1_TRNG_RAND_BASE_OFFSET + (BYTES_IN_WORD * inx));
		LTRACEF("TRNG res %u = 0x%x\n", residue, val);

		for (; inx < residue; inx++) {
			p[inx] = BYTE_VALUE(val);
			val = val >> 8U;
		}
	}

	val = 0U;
	FI_BARRIER(u32, val);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
/* Generates max TRNG_BYTES of TRNG value to dst in bytes
 * in this call.
 *
 * If bytes/dst is zero, generate random, but copy nothing
 * (clears engine regs from previous call).
 */
static status_t pka1_trng_gen_rand(const engine_t *engine,
				   uint8_t *dst,
				   uint32_t bytes)
{
	status_t ret = NO_ERROR;
	uint32_t val = 0U;
	uint32_t polls = 0;

	LTRACEF("entry\n");

	/* Can only read TRNG_WORDS/TRNG_BYTES from HW TRNG in this call */
	if (bytes > TRNG_BYTES) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	while (polls < MAX_TRNG_POLLS) {
		val = tegra_engine_get_reg(engine, SE_PKA1_TRNG_STATUS_OFFSET);
		if ((val & SE_PKA1_TRNG_STATUS_SEEDED(SE_ELP_ENABLE)) != 0U) {
			break;
		}
		polls++;
	}

	if (MAX_TRNG_POLLS == polls) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("TRNG not seeded\n"));
	}

	val = SE_PKA1_TRNG_CTRL_CMD(SE_PKA1_TRNG_CTRL_CMD_GENERATE);
	tegra_engine_set_reg(engine, SE_PKA1_TRNG_CTRL_OFFSET, val);

	polls = 0U;
	while (polls < MAX_TRNG_POLLS) {
		val = tegra_engine_get_reg(engine, SE_PKA1_TRNG_ISTAT_OFFSET);
		if ((val & SE_PKA1_TRNG_ISTAT_RAND_RDY(SE_ELP_ENABLE)) != 0U) {
			break;
		}
		polls++;
	}

	if (MAX_TRNG_POLLS == polls) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("TRNG not completed\n"));
	}

	if ((0U != bytes) && (NULL != dst)) {
		ret = trng_copy_bytes(engine, bytes, dst);
		CCC_ERROR_CHECK(ret);
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t pka1_trng_generate_locked(const struct engine_s *engine,
				   uint8_t *buf, uint32_t blen)
{
	status_t ret = NO_ERROR;
	uint8_t *p = buf;
	bool flush = false;
	uint32_t tchunks = blen / TRNG_BYTES;
	uint32_t residue = blen % TRNG_BYTES;

	LTRACEF("entry\n");

	if ((NULL == engine) || (NULL == buf) || (0U == blen)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = pka1_trng_setup_locked(engine);
	CCC_ERROR_CHECK(ret);

	flush = true;

	while (tchunks > 0U) {
		ret = pka1_trng_gen_rand(engine, p, TRNG_BYTES);
		if (NO_ERROR != ret) {
			break;
		}
		p = &p[TRNG_BYTES];
		tchunks--;
	}
	CCC_ERROR_CHECK(ret);

	if (residue > 0U) {
		ret = pka1_trng_gen_rand(engine, p, residue);
		CCC_ERROR_CHECK(ret);
	}

fail:
	if (BOOL_IS_TRUE(flush)) {
		/* For security reasons overwrite the generated
		 * RAND from TRNG regs
		 */
		(void)pka1_trng_gen_rand(engine, NULL, 0U);
	}
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Called via cdev p_rng field, engine class TRNG */
status_t engine_pka1_trng_generate_locked(
	struct se_data_params *input_params,
	const struct se_engine_rng_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == input_params)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = pka1_trng_generate_locked(econtext->engine, input_params->dst,
					input_params->output_size);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if defined(NEED_PKA1_TRNG_GENERATE_RANDOM)
status_t pka1_trng_generate_random(uint8_t *buf, uint32_t size)
{
	status_t ret = NO_ERROR;
	const engine_t *engine = NULL;

	LTRACEF("entry\n");

	ret = ccc_select_engine(&engine, CCC_ENGINE_CLASS_TRNG, ENGINE_PKA);
	CCC_ERROR_CHECK(ret);

	HW_MUTEX_TAKE_ENGINE(engine);
	ret = pka1_trng_generate_locked(engine, buf, size);
	HW_MUTEX_RELEASE_ENGINE(engine);

	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* defined(NEED_PKA1_TRNG_GENERATE_RANDOM) */

#if HAVE_CLOSE_FUNCTIONS
void engine_pka1_trng_close_locked(void)
{
}
#endif
#endif /* HAVE_PKA1_TRNG */
