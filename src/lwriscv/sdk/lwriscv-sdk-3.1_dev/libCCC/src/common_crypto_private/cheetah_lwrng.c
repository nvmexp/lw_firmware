/*
 * Copyright (c) 2017-2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */
#include <crypto_system_config.h>

#if HAVE_LWRNG_DRNG

#include <ccc_lwrm_drf.h>
#include <dev_lwrng.h>

#include <crypto_ops.h>
#include <tegra_lwrng.h>

#if 0
/* Define this if the system running this supports RSTAT and IRSTAT
 * registers and the code should poll those like in SE IAS
 * page 184 psc_se_init_step
 *
 * CMOD/VDK does not seem to support those so can not test this.
 *
 * CMOD does not need this undef anymore => fixed. Remove this
 * once verified.
 *
 * undef POLL_LWRNG_RSTAT_STARTUP
 */
#endif /* 0 */

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#define MEASURE_PERF_ENGINE_LWRNG_OPERATION	0

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_LWRNG_OPERATION)
static void show_rng1_cmd_entry_perf(uint32_t elapsed, uint32_t cmd)
{
	CCC_ERROR_MESSAGE("LWRNG engine[cmd %u]: %u usec\n", cmd, elapsed);
}
#endif

static uint32_t lwrng_status_good;

static void lwrng_set_status_unknown(void)
{
	lwrng_status_good = 0U;
}

/**
 * @brief Reset LWRNG device.
 *
 * @param engine engine to reset (LWRNG engine)
 *
 * @return NO_ERROR on success,
 *  ERR_TIMED_OUT : On reset timeout error
 *  ERR_ILWALID_ARGS : Engine is not CCC_ENGINE_LWRNG
 */
status_t lwrng_reset(const struct engine_s *engine)
{
	status_t ret    = NO_ERROR;
	uint32_t regval = 0U;
	uint32_t rmask  = 0U;
	uint32_t count  = 0U;

	LTRACEF("entry\n");

	lwrng_set_status_unknown();

	regval = LW_DRF_DEF(LW_LWRNG, R_CTRL1, SOFT_RST, TRUE);
	rmask = regval;
	tegra_engine_set_reg(engine, LW_LWRNG_R_CTRL1_0, regval);

	/* HW clears SOFT_RST bit once reset complete
	 */
	do {
		if (count > LWRNG_RESET_TIMEOUT) {
			ret = SE_ERROR(ERR_TIMED_OUT);
			break;
		}
		count++;

		regval = tegra_engine_get_reg_NOTRACE(engine, LW_LWRNG_R_CTRL1_0);
	} while ((regval & rmask) != 0U);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if defined(POLL_LWRNG_RSTAT_STARTUP)
/* This would block for < 2000 cycles if LWRNG setup health tests take that long
 * after LWRNG leaves reset (assuming STARTUP_DONE not set before this call).
 *
 * But neither R_ISTAT register nor R_STAT are implemented in CMOD
 * => can not call this yet.
 */
static status_t lwrng_wait_for_init_step_done(const struct engine_s *engine)
{
	status_t ret = NO_ERROR;
	uint32_t count = 1U;
	uint32_t stat = 0U;
	uint32_t regval = 0U;
	const uint32_t istat_done = LW_LWRNG_R_ISTAT_0_STARTUP_DONE_TRUE;

	LTRACEF("entry\n");

	/* Poll until R_ISTATE field STARTUP_DONE is set or timeout.
	 */
	do {
		if (count > LWRNG_STARTUP_RETRY_COUNT) {
			ret = SE_ERROR(ERR_BAD_STATE);
			regval = tegra_engine_get_reg(engine, LW_LWRNG_R_STAT_0);
			LTRACEF("LWRNG startup failed, R_STAT 0x%x\n", regval);
			break;
		}
		count++;

		regval = tegra_engine_get_reg_NOTRACE(engine, LW_LWRNG_R_ISTAT_0);
		stat = LW_DRF_VAL(LW_LWRNG, R_ISTAT, STARTUP_DONE, regval);
	} while (stat != istat_done);
	CCC_ERROR_CHECK(ret);

	/* Read R_STAT register for STARTUP_ERROR field value */
	regval = tegra_engine_get_reg(engine, LW_LWRNG_R_STAT_0);
	stat = LW_DRF_VAL(LW_LWRNG, R_STAT, STARTUP_ERROR, regval);
	if (stat != LW_LWRNG_R_STAT_0_STARTUP_ERROR_FALSE) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* defined(POLL_LWRNG_RSTAT_STARTUP) */

static status_t lwrng_check_status(const struct engine_s *engine)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	(void)engine;

#if defined(POLL_LWRNG_RSTAT_STARTUP)
	/* This is necessary after LWRNG exits reset
	 * i.e. at startup and soft resets.
	 */
	ret = lwrng_wait_for_init_step_done(engine);
	CCC_ERROR_CHECK(ret);
	lwrng_status_good = 0x5A5A5AU;
fail:
#else
	lwrng_status_good = 0x5A5A5AU;
#endif

	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Each read produces 16 bit random value, each word contains
 * validity bit.
 */
/* Each read produces 16 bit random value, each word contains
 * validity bit.
 *
 * If the VALID bit is not set, retry a fixed number of times just in case
 * the HW is temporarily out of entropy. If so, the retries would improve
 * sucess rate.
 *
 * XXXX TODO: Ask HW team for number of recommended retries!
 */
static inline bool lwrng_drbg16(const struct engine_s *engine,
				uint16_t *rv)
{
	uint32_t val = 0U;
	uint32_t regval = 0U;
	uint32_t retries = LWRNG_DFIFO_RETRY_COUNT;
	bool valid = false;
	const uint32_t vmask = LW_DRF_DEF(LW_LWRNG, DFIFO_OUT, VALID, TRUE);
	CCC_LOOP_VAR;

	CCC_LOOP_BEGIN {
		val = tegra_engine_get_reg(engine, LW_LWRNG_DFIFO_OUT_0);
		regval = val;

		/* Misra obslwrities */
		if (val >= 0x10000U) {
			val &= 0xFFFFU;
		}

		valid = ((regval & vmask) != 0U);
		if (BOOL_IS_TRUE(valid)) {
			CCC_LOOP_STOP;
		}

		if (0U == retries) {
			LTRACEF("LWRNG values invalid w/retries\n");
			CCC_LOOP_STOP;
		}
		retries = retries - 1U;
	} CCC_LOOP_END;

	*rv = (uint16_t)val;
	return valid;
}

static status_t lwrng_get_random_bytes(const struct engine_s *engine,
				       uint8_t *rbuf, uint32_t nbytes)
{
	status_t ret = NO_ERROR;
	uint32_t half_count = nbytes / sizeof_u32(uint16_t);
	uint32_t residue = nbytes % sizeof_u32(uint16_t);
	uint32_t inx = 0U;
	uint16_t v16 = 0U;
	uint8_t *p = NULL;

	LTRACEF("entry (%u bytes)\n", nbytes);

	if ((NULL == rbuf) || (0U == nbytes)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (0U == lwrng_status_good) {
		ret = lwrng_check_status(engine);
		CCC_ERROR_CHECK(ret);
	}

	p = &rbuf[0];

	for (; inx < half_count; inx++) {
		uint32_t v = 0U;
		if (!BOOL_IS_TRUE(lwrng_drbg16(engine, &v16))) {
			ret = SE_ERROR(ERR_NOT_VALID);
			break;
		}

		v = (uint32_t)v16 >> 8U;
		*p = BYTE_VALUE(v);
		p++;
		*p = BYTE_VALUE(v16);
		p++;
	}
	CCC_ERROR_CHECK(ret);

	if (residue != 0U) {
		if (!BOOL_IS_TRUE(lwrng_drbg16(engine, &v16))) {
			CCC_ERROR_WITH_ECODE(ERR_NOT_VALID);
		}
		*p = BYTE_VALUE(v16);
		p++;
	}

	LOG_HEXBUF("LWRNG out", rbuf, nbytes);
fail:
	v16 = 0U;
	LTRACEF("exit: %d\n", ret);

	return ret;
}

/* Called via cdev p_rng field, engine class TE_CLASS_DRNG
 */
status_t engine_lwrng_drng_generate_locked(
	struct se_data_params *input_params,
	const struct se_engine_rng_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == input_params)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (CCC_ENGINE_RNG1 != econtext->engine->e_id) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = lwrng_get_random_bytes(econtext->engine,
				     input_params->dst,
				     input_params->output_size);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_CLOSE_FUNCTIONS
void engine_lwrng_drng_close_locked(void)
{
}
#endif

/* Call this to fetch randomness to buffer when CCC is used as library
 * for "kernel code" (same address space calls).
 *
 * This manages the mutex and selects the proper engine internally.
 *
 * NOTE: This function uses the LWRNG mutex locks directly (does not use the
 * HW_MUTEX_TAKE_ENGINE/HW_MUTEX_RELEASE_ENGINE macros) so that this function
 * can be always called no matter what other mutexes are held by the caller.
 *
 * This requires that the LWRNG mutex is not granted to anyone if it is lwrrently
 * held (by anyone)! I think the HW works this way => please let me know if
 * you disagree!
 *
 * If this is not true, then operations that already hold some mutex would be
 * required to call DRNG locked function protected by the same mutex: SE => SE0_DRNG,
 * PKA1 => TRNG and LWRNG could be only called when no mutexes are held (as the
 * macros lock the "CCC system SW mutex", they prevent such cross-mutex calls!)
 */
status_t lwrng_generate_drng(uint8_t *dbuf,
			     uint32_t dbuf_len)
{
	status_t ret = NO_ERROR;
	const struct engine_s *engine = NULL;

	LTRACEF("entry\n");

	ret = ccc_select_device_engine(SE_CDEV_RNG1, &engine, CCC_ENGINE_CLASS_DRNG);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to select LWRNG engine\n"));

	ret = lwrng_get_random_bytes(engine, dbuf, dbuf_len);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_LWRNG_DRNG */
