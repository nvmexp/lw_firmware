/*
 * Copyright (c) 2017-2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/* Support CheetAh Security Engine Elliptic core : random numbers.
 *
 * Supports Elliptic RNG1 device
 *
 * Elliptic CLP-890 is advertised to be NIST SP800-90a/b/c and BSI AIS
 * 20/31 compliant Random number generator.
 *
 * The circuit includes NIST SP800-90b compliant noise source,
 * a NIST SP800-90b approved component and a NIST SP800-90a approved
 * Deterministic Random Number Generator (DRBG).
 */
#include <crypto_system_config.h>

#if HAVE_RNG1_DRNG

#ifndef TEGRA_UDELAY_VALUE_RNG1_MUTEX
#define TEGRA_UDELAY_VALUE_RNG1_MUTEX 0
#endif

#include <ccc_lwrm_drf.h>
#include <arse_rng1.h>

#include <crypto_ops.h>
#include <tegra_rng1.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#define MEASURE_PERF_ENGINE_RNG1_OPERATION	0

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_RNG1_OPERATION)
static void show_rng1_cmd_entry_perf(uint32_t elapsed, uint32_t cmd)
{
	CCC_ERROR_MESSAGE("RNG1 engine[cmd %u]: %u usec\n", cmd, elapsed);
}
#endif

/* Apparently makes randomness less predicatable, but also adds
 * commands (GEN_NOISE, RENEW_STATE) to the generation chain
 * (i.e. makes it slower).
 *
 * For our purposes: should keep the prediction resistance on
 */
#ifndef HAVE_RNG1_PREDICTION_RESISTANCE
#define HAVE_RNG1_PREDICTION_RESISTANCE 1
#endif

#ifndef RNG1_RESET_TIMEOUT
#define RNG1_RESET_TIMEOUT 0x100000U
#endif

#ifndef RNG1_WATCHDOG_TIMEOUT
#define RNG1_WATCHDOG_TIMEOUT 0xFFFFFFFFU
#endif

/**
 * @brief Reset RNG1 device.
 *
 * @param engine engine to reset (RNG1 engine)
 *
 * @return NO_ERROR on success,
 *  ERR_TIMED_OUT : On reset timeout error
 *  ERR_ILWALID_ARGS : Engine is not CCC_ENGINE_RNG1
 */
status_t rng1_reset(const engine_t *engine)
{
	status_t ret = NO_ERROR;
	uint32_t val = 0U;
	uint32_t count = 0U;

	LTRACEF("entry\n");

	if (CCC_ENGINE_RNG1 != engine->e_id) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	val = LW_DRF_DEF(SE_RNG1, RNG1_ECTL, RST, TRUE);
	tegra_engine_set_reg(engine, SE_RNG1_RNG1_ECTL_0, val);

	do {
		uint32_t rval = tegra_engine_get_reg(engine, SE_RNG1_RNG1_ECTL_0);

		if ((val & rval) == 0U) {
			LTRACEF("RNG1 reset completed: count %u\n", count);
			break;
		}

		count++;

		if (count > RNG1_RESET_TIMEOUT) {
			CCC_ERROR_MESSAGE("RNG1 reset timeout\n");
			ret = SE_ERROR(ERR_TIMED_OUT);
		}
	} while (NO_ERROR == ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* RNG1 MUTEX HANDLERS */

status_t rng1_acquire_mutex(const struct engine_s *engine)
{
	status_t ret = NO_ERROR;
	uint32_t val = 0U;
	uint32_t i = 0U;

	LTRACEF("entry\n");

	/* Acquire rng1 mutex */
	do {
		/* loop count check for mutex lock timeout */
		if (i > SE_RNG1_TIMEOUT) {
			CCC_ERROR_MESSAGE("Acquire RNG1 Mutex timed out\n");
			ret = SE_ERROR(ERR_TIMED_OUT);
		} else {
			// It is not possible to trace this register get (may loop a lot of times)
			val = tegra_engine_get_reg_NOTRACE(engine, SE_RNG1_RNG1_MUTEX_REQUEST_RELEASE_0);
			if ((0x01U & val) != 0U) {
				break;
			}

#if TEGRA_UDELAY_VALUE_RNG1_MUTEX > 0
			TEGRA_UDELAY(TEGRA_UDELAY_VALUE_RNG1_MUTEX);
#endif

			i++;
		}
	} while (NO_ERROR == ret);
	CCC_ERROR_CHECK(ret, CCC_ERROR_MESSAGE("RNG1 mutex lock failed\n"));

	tegra_engine_set_reg(engine, SE_RNG1_RNG1_MUTEX_TIMEOUT_ACTION_0,
			     SE_RNG1_MUTEX_TIMEOUT_ACTION);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

void rng1_release_mutex(const struct engine_s *engine)
{
	LTRACEF("entry\n");
	tegra_engine_set_reg(engine, SE_RNG1_RNG1_MUTEX_REQUEST_RELEASE_0, 0x01U);
	LTRACEF("exit\n");
}

#if MODULE_TRACE
static void rng1_show_status(const engine_t *engine) {
	LTRACEF("entry\n");
	LTRACEF("RNG1 status regs:\n");
	LTRACEF(" RNG1_MODE_0   = 0x%x\n",     tegra_engine_get_reg_NOTRACE(engine, SE_RNG1_RNG1_MODE_0));
	LTRACEF(" RNG1_SMODE_0  = 0x%x\n",     tegra_engine_get_reg_NOTRACE(engine, SE_RNG1_RNG1_SMODE_0));
	LTRACEF(" RNG1_STAT_0   = 0x%x\n",     tegra_engine_get_reg_NOTRACE(engine, SE_RNG1_RNG1_STAT_0));
	LTRACEF(" RNG1_IE_0     = 0x%x\n",     tegra_engine_get_reg_NOTRACE(engine, SE_RNG1_RNG1_IE_0));
	LTRACEF(" RNG1_ISTAT_0  = 0x%x\n",     tegra_engine_get_reg_NOTRACE(engine, SE_RNG1_RNG1_ISTAT_0));
	LTRACEF(" RNG1_ALARMS_0 = 0x%x\n",     tegra_engine_get_reg_NOTRACE(engine, SE_RNG1_RNG1_ALARMS_0));
	LTRACEF(" RNG1_MUTEX_STATUS_0 = 0x%x\n", tegra_engine_get_reg_NOTRACE(engine, SE_RNG1_RNG1_MUTEX_STATUS_0));
	LTRACEF(" RNG1_INT_STATUS_0   = 0x%x\n", tegra_engine_get_reg_NOTRACE(engine, SE_RNG1_RNG1_INT_STATUS_0));
	LTRACEF(" RNG1_SELWRITY_0   = 0x%x\n", tegra_engine_get_reg_NOTRACE(engine, SE_RNG1_RNG1_SELWRITY_0));
	LTRACEF("exit\n");
}
#endif

static status_t rng1_wait_for_idle(const engine_t *engine)
{
	status_t ret = NO_ERROR;
	uint32_t status = 0U;
	uint32_t timeout = 0U;
	uint32_t val = 0U;

	LTRACEF("entry\n");

	do {
		val = tegra_engine_get_reg_NOTRACE(engine, SE_RNG1_RNG1_STAT_0);

		status = LW_DRF_VAL(SE_RNG1, RNG1_STAT, BUSY, val);
		if ((status & SE_RNG1_RNG1_STAT_0_BUSY_TRUE) == 0U) {
			LTRACEF("RNG1 stat register 0x%x (when not busy)\n", val);
			break;
		}

		val = tegra_engine_get_reg_NOTRACE(engine, SE_RNG1_RNG1_INT_STATUS_0);
		timeout = LW_DRF_VAL(SE_RNG1, RNG1_INT_STATUS, MUTEX_TIMEOUT, val);

		if (timeout != 0U) {
			(void)rng1_reset(engine);
			ret = SE_ERROR(ERR_TIMED_OUT);
		}
	} while (NO_ERROR == ret);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rng1_check_alarms(const engine_t *engine)
{
	status_t ret = NO_ERROR;
	uint32_t val = 0U;

	LTRACEF("entry\n");

	val = tegra_engine_get_reg(engine, SE_RNG1_RNG1_ALARMS_0);
	if (0U != val) {
		CCC_ERROR_MESSAGE("RNG1 alarms detected: 0x%x\n", val);
#if SE_RD_DEBUG
		if (LW_DRF_VAL(SE_RNG1, RNG1_ALARMS, ILLEGAL_CMD_SEQ, val) != 0U) {
			CCC_ERROR_MESSAGE("RNG1: illegal command sequence: SW issue\n");
		}

		uint32_t failed_test = LW_DRF_VAL(SE_RNG1, RNG1_ALARMS, FAILED_TEST_ID, val);
		if (failed_test != 0U) {
			CCC_ERROR_MESSAGE("RNG1: Failed test ID: 0x%x\n", failed_test);
		}
#endif
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rng1_wait_cmd_complete(const engine_t *engine,
				       uint32_t command,
				       uint32_t rng1_istat_ready_mask)
{
	status_t ret = NO_ERROR;
	uint32_t int_status = 0U;
	uint32_t rng1_istat  = 0U;

	(void)command;

	LTRACEF("entry\n");

	if ((NULL == engine) || (0U == rng1_istat_ready_mask)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* CMD is ready when the CMD specific mask bit turns 1 in ISTAT register
	 * For most commands this is DONE, but KAT and ZEROIZE use other bits.
	 *
	 * In case we have a mutex timeout "interrupt" => reset RNG1 and return error.
	 */
	do {
		rng1_istat = tegra_engine_get_reg_NOTRACE(engine, SE_RNG1_RNG1_ISTAT_0);
		if ((rng1_istat & rng1_istat_ready_mask) != 0U) {
			break;
		}

		/* Check mutex timeout */
		int_status = tegra_engine_get_reg_NOTRACE(engine, SE_RNG1_RNG1_INT_STATUS_0);

		if ((0U != LW_DRF_VAL(SE_RNG1, RNG1_INT_STATUS, MUTEX_TIMEOUT, int_status)) ||
		    (0U != LW_DRF_VAL(SE_RNG1, RNG1_INT_STATUS, ERROR, int_status))) {

			LTRACEF("RNG1 int status: 0x%x\n", int_status);

			/* clear the writable INT_STATUS bits */
			tegra_engine_set_reg(engine, SE_RNG1_RNG1_INT_STATUS_0, int_status);

			(void)rng1_reset(engine);
			ret = SE_ERROR(ERR_TIMED_OUT);
		}
	} while (NO_ERROR == ret);
	CCC_ERROR_CHECK(ret);

#if MODULE_TRACE
	LTRACEF("Status after command 0x%x idle\n", command);
	rng1_show_status(engine);
#endif

	/* Not sure if normal commands do trigger these alarms?
	 * KAT tests probably do so check anyway.
	 */
	if (LW_DRF_VAL(SE_RNG1, RNG1_ISTAT, ALARMS, rng1_istat) != 0U) {
		CCC_ERROR_MESSAGE("Command 0x%x completed with alarms?\n", command);
		(void)rng1_check_alarms(engine);

		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

fail:
	if (NULL != engine) {
		/* Clear ISTAT bits before returning */
		tegra_engine_set_reg(engine, SE_RNG1_RNG1_ISTAT_0, rng1_istat);
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rng1_cmd(const engine_t *engine, uint32_t cmd)
{
	status_t ret     = NO_ERROR;
	uint32_t command = 0U;
	uint32_t rng1_istat  = 0U;
	uint32_t rng1_istat_ready_mask = LW_DRF_DEF(SE_RNG1, RNG1_ISTAT, DONE, ACTIVE);

	LTRACEF("entry\n");

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_RNG1_OPERATION)
	uint32_t cmd_perf_usec = GET_USEC_TIMESTAMP;
#endif

	/* If SE_RNG1_RNG1_SELWRITY_0 bit 1:1 is set => RNG1 is disabled
	 * and all commands would fail. After testing this it seems that if the
	 * command is started they never complete, so it is best to check this
	 * condition before starting a command and fail it here.
	 */
	{
		uint32_t eng_dis = tegra_engine_get_reg(engine, SE_RNG1_RNG1_SELWRITY_0);
		if ((eng_dis & LW_DRF_DEF(SE_RNG1, RNG1_SELWRITY, SE_ENG_DIS, TRUE)) != 0U) {
			LTRACEF("RNG1 device disabled until next system reset\n");
			CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
		}
	}

	command = LW_DRF_NUM(SE_RNG1, RNG1_CTRL, CMD, cmd);

	switch (command) {
	case LW_DRF_DEF(SE_RNG1, RNG1_CTRL, CMD, NOP):
		LTRACEF("RNG1 command (%d): %s\n", command, "NOP");
		break;
	case LW_DRF_DEF(SE_RNG1, RNG1_CTRL, CMD, GEN_NOISE):
		LTRACEF("RNG1 command (%d): %s\n", command, "GEN_NOISE");
		break;
	case LW_DRF_DEF(SE_RNG1, RNG1_CTRL, CMD, GEN_NONCE):
		LTRACEF("RNG1 command (%d): %s\n", command, "GEN_NONCE");
		break;
	case LW_DRF_DEF(SE_RNG1, RNG1_CTRL, CMD, CREATE_STATE):
		LTRACEF("RNG1 command (%d): %s\n", command, "CREATE_STATE");
		break;
	case LW_DRF_DEF(SE_RNG1, RNG1_CTRL, CMD, RENEW_STATE):
		LTRACEF("RNG1 command (%d): %s\n", command, "RENEW_STATE");
		break;
	case LW_DRF_DEF(SE_RNG1, RNG1_CTRL, CMD, REFRESH_ADDIN):
		LTRACEF("RNG1 command (%d): %s\n", command, "REFRESH_ADDIN");
		break;
	case LW_DRF_DEF(SE_RNG1, RNG1_CTRL, CMD, GEN_RANDOM):
		LTRACEF("RNG1 command (%d): %s\n", command, "GEN_RANDOM");
		break;
	case LW_DRF_DEF(SE_RNG1, RNG1_CTRL, CMD, ADVANCE_STATE):
		LTRACEF("RNG1 command (%d): %s\n", command, "ADVANCE_STATE");
		break;
	case LW_DRF_DEF(SE_RNG1, RNG1_CTRL, CMD, KAT):
		LTRACEF("RNG1 command (%d): %s\n", command, "KAT");
		rng1_istat_ready_mask = LW_DRF_DEF(SE_RNG1, RNG1_ISTAT, KAT_COMPLETED, ACTIVE);
		break;
	case LW_DRF_DEF(SE_RNG1, RNG1_CTRL, CMD, ZEROIZE):
		LTRACEF("RNG1 command (%d): %s\n", command, "ZEROIZE");
		rng1_istat_ready_mask = LW_DRF_DEF(SE_RNG1, RNG1_ISTAT, ZEROIZED, ACTIVE);
		break;
	default:
		CCC_ERROR_MESSAGE("Unsupported RNG1 command: %d\n", command);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	/* Clear the rng1_istat before starting the next command */
	rng1_istat = tegra_engine_get_reg(engine, SE_RNG1_RNG1_ISTAT_0);
	tegra_engine_set_reg(engine, SE_RNG1_RNG1_ISTAT_0, rng1_istat);

	/* Start the command */
	tegra_engine_set_reg(engine, SE_RNG1_RNG1_CTRL_0, command);

	ret = rng1_wait_cmd_complete(engine, command, rng1_istat_ready_mask);
	CCC_ERROR_CHECK(ret);

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_RNG1_OPERATION)
	show_rng1_cmd_entry_perf(get_elapsed_usec(cmd_perf_usec), cmd);
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rng1_setup(const engine_t *engine)
{
	status_t ret = NO_ERROR;
	uint32_t val = 0U;

	LTRACEF("entry\n");

#if MODULE_TRACE
	LTRACEF("RNG1 engine status before command:\n");
	rng1_show_status(engine);
#endif

	/* Step 3: make sure RNG1 is idle */
	/* Make sure RNG1 is idle */
	ret = rng1_wait_for_idle(engine);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("RNG1 is not idle\n"));

	/* Set RNG1 watchdog timeout
	 */
	tegra_engine_set_reg(engine, SE_RNG1_RNG1_MUTEX_WATCHDOG_COUNTER_0,
			     RNG1_WATCHDOG_TIMEOUT);

	/* Clear INT_STATUS ERROR field before starting a new operation
	 */
	val = 0U;
	val = LW_FLD_SET_DRF_DEF(SE_RNG1, RNG1_INT_STATUS, ERROR, SW_CLEAR, val);
	val = LW_FLD_SET_DRF_DEF(SE_RNG1, RNG1_INT_STATUS, MUTEX_TIMEOUT, SW_CLEAR, val);
	tegra_engine_set_reg(engine, SE_RNG1_RNG1_INT_STATUS_0, val);

	/* step 5: ZEROIZE command */
	ret = rng1_cmd(engine, SE_RNG1_RNG1_CTRL_0_CMD_ZEROIZE);
	CCC_ERROR_CHECK(ret);

	/* Disable interrupts */
	tegra_engine_set_reg(engine, SE_RNG1_RNG1_IE_0, 0x0U);
	tegra_engine_set_reg(engine, SE_RNG1_RNG1_INT_ENABLE_0, 0x0U);

	/* Step 6: set SMODE */
	/* SMODE reg:
	 *
	 * SECURE : 1
	 *
	 * first, then set SMODE reg:
	 *
	 * SECURE : 1
	 * NONCE  : 0
	 * MAX_REJECTS : 0xa
	 *
	 * SMODE must be set in two writes because the SECURE setting forces
	 * defaults to other fields => first set secure mode, then set the
	 * other SMODE fields.
	 */

	/* Set SELWRE_EN to SECURE state and set the other bits in the
	 * separate write to the same register.
	 */
	val = LW_DRF_DEF(SE_RNG1, RNG1_SMODE, SELWRE_EN, SECURE);
	tegra_engine_set_reg(engine, SE_RNG1_RNG1_SMODE_0, val);

	/* Leave SELWRE_EN to SECURE in VAL for the next write as well
	 */
	val = LW_FLD_SET_DRF_DEF(SE_RNG1, RNG1_SMODE, NONCE,	   DISABLE, val);
	val = LW_FLD_SET_DRF_DEF(SE_RNG1, RNG1_SMODE, MAX_REJECTS, DEFAULT, val);
	tegra_engine_set_reg(engine, SE_RNG1_RNG1_SMODE_0, val);

	/* Step 7: set RNG1_MODE */
	/* Write RNG1_MODE:
	 *
	 * ADDIN_PRESENT : 0 (false)
	 * PRED_RESIST   : 1 (true) (if pred resistance configured)
	 * KAT_SEL       : 0 (DRBG)
	 * KAT0          : 0 (KAT0)
	 *   if HAVE_RNG1_USE_AES256
	 * SEC_ALG       : 1 (AES_256)
	 *   else
	 * SEC_ALG       : 0 (AES_128)
	 *   endif
	 */
	val = 0U;
	val = LW_FLD_SET_DRF_DEF(SE_RNG1, RNG1_MODE, ADDIN_PRESENT, FALSE,   val);
#if HAVE_RNG1_PREDICTION_RESISTANCE
	val = LW_FLD_SET_DRF_DEF(SE_RNG1, RNG1_MODE, PRED_RESIST,   TRUE,    val);
#else
	val = LW_FLD_SET_DRF_DEF(SE_RNG1, RNG1_MODE, PRED_RESIST,   FALSE,    val);
#endif
	val = LW_FLD_SET_DRF_DEF(SE_RNG1, RNG1_MODE, KAT_SEL,       DRBG,    val);
#if HAVE_RNG1_USE_AES256
	val = LW_FLD_SET_DRF_DEF(SE_RNG1, RNG1_MODE, SEC_ALG,       AES_256, val);
#else
	val = LW_FLD_SET_DRF_DEF(SE_RNG1, RNG1_MODE, SEC_ALG,       AES_128, val);
#endif

	tegra_engine_set_reg(engine, SE_RNG1_RNG1_MODE_0, val);

	/* Step 8: GEN_NOISE */
	ret = rng1_cmd(engine, SE_RNG1_RNG1_CTRL_0_CMD_GEN_NOISE);
	CCC_ERROR_CHECK(ret);

	/* Step 9: CREATE_STATE */
	ret = rng1_cmd(engine, SE_RNG1_RNG1_CTRL_0_CMD_CREATE_STATE);
	CCC_ERROR_CHECK(ret);

#if HAVE_RNG1_PREDICTION_RESISTANCE
	ret = rng1_cmd(engine, SE_RNG1_RNG1_CTRL_0_CMD_GEN_NOISE);
	CCC_ERROR_CHECK(ret);

	ret = rng1_cmd(engine, SE_RNG1_RNG1_CTRL_0_CMD_RENEW_STATE);
	CCC_ERROR_CHECK(ret);
#endif /* HAVE_RNG1_PREDICTION_RESISTANCE */

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}


/* Generate 16 bytes of random to the RNG1 register block
 */
static status_t rng1_gen_new_random(const engine_t *engine)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = rng1_cmd(engine, SE_RNG1_RNG1_CTRL_0_CMD_GEN_RANDOM);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rng1_get_random_data_locked(const engine_t *engine,
				     uint8_t *dbuf,
				     uint32_t dbuf_len)
{
	status_t ret = NO_ERROR;
	uint8_t *dst = dbuf;
	uint32_t chunks = 0U;
	uint32_t count  = 0U;
	uint32_t nwords = 0U;

	/* RNG1_DRNG_BYTES is a multiple of 4 bytes (16 bytes).
	 * Read word by word and write to dbuf.
	 *
	 * Can not use stack buffers for move optimizations due to
	 * safety requirements.
	 */
	uint32_t rnd = 0U;

	LTRACEF("entry\n");

	/* XXX TODO: Should enforce some defined max size as well
	 */
	if ((0U == dbuf_len) || (dbuf == NULL)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	count = dbuf_len / RNG1_DRNG_BYTES;

	for (chunks = 0U; chunks < count; chunks++) {
		/* Step 10: generate new random to RNG1_RAND registers */
		ret = rng1_gen_new_random(engine);
		CCC_ERROR_CHECK(ret);

		/* Step 11: Read words from RNG1_RAND0 */
		for (nwords = 0U; nwords < (RNG1_DRNG_BYTES / BYTES_IN_WORD); nwords++) {
			rnd = tegra_engine_get_reg(engine,
						   (SE_RNG1_RNG1_RAND0_0 + (BYTES_IN_WORD * nwords)));
			se_util_u32_to_buf(rnd, &dst[BYTES_IN_WORD * nwords], false);
		}

		dst = &dst[RNG1_DRNG_BYTES];
	}

	/* Nonzero if we need one more chunk of values which will get
	 * partly consumed. Value is number of bytes to consume from that chunk.
	 */
	count = dbuf_len % RNG1_DRNG_BYTES;

	if (0U != count) {
		/* Full words in the last incomplete chunk */
		uint32_t full_words = count / BYTES_IN_WORD;

		/* Step 10: generate new random chunk of values to RNG1_RAND registers */
		ret = rng1_gen_new_random(engine);
		CCC_ERROR_CHECK(ret);

		/* Step 11: Partly consume the last chunk of randoms.
		 *
		 * First copy full words, then fetch last word and copy residual bytes
		 * to DST buffer.
		 */
		for (nwords = 0U; nwords < full_words; nwords++) {
			rnd = tegra_engine_get_reg(engine,
						   (SE_RNG1_RNG1_RAND0_0 + (BYTES_IN_WORD * nwords)));
			se_util_u32_to_buf(rnd, &dst[BYTES_IN_WORD * nwords], false);
		}

		dst = &dst[full_words * BYTES_IN_WORD];

		/* Last random value possibly used to fill residual bytes
		 */
		rnd = tegra_engine_get_reg(engine,
					   (SE_RNG1_RNG1_RAND0_0 + (BYTES_IN_WORD * full_words)));

		/* Number of residual bytes from last word of random
		 */
		count = count % BYTES_IN_WORD;

		/* Move the residual bytes of the last incomplete word to dst,
		 * if any.
		 */
		if (0U != count) {
			uint32_t inx = 0U;

			for (inx = 0U; inx < count; inx++) {
				dst[inx] = BYTE_VALUE(rnd);
				rnd = rnd >> 8U;
			}
		}
	}

fail:
	rnd = 0U;
	FI_BARRIER(u32, rnd);

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rng1_generate_locked(const engine_t *engine,
				     uint8_t *dbuf,
				     uint32_t dbuf_len)
{
	status_t ret = NO_ERROR;
	bool clear_state = false;

	LTRACEF("entry\n");

	ret = rng1_setup(engine);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("RNG1 setup failed\n"));

	/* Clear the RNG1 state after this point */
	clear_state = true;

	ret = rng1_get_random_data_locked(engine, dbuf, dbuf_len);
	CCC_ERROR_CHECK(ret);

	/* Step 12: repeated steps 10 && 11 to get enough rand bytes */

	/* Step 14: Advance state */
	(void)rng1_cmd(engine, SE_RNG1_RNG1_CTRL_0_CMD_ADVANCE_STATE);

fail:
	if (BOOL_IS_TRUE(clear_state)) {
		/* Must not leave any generated random to RNG1 registers
		 */
		(void)rng1_cmd(engine, SE_RNG1_RNG1_CTRL_0_CMD_ZEROIZE);
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Called via cdev p_rng field, engine class TE_CLASS_DRNG
 */
status_t engine_rng1_drng_generate_locked(
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

	ret = rng1_generate_locked(econtext->engine, input_params->dst,
				   input_params->output_size);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_CLOSE_FUNCTIONS
void engine_rng1_drng_close_locked(void)
{
}
#endif

/* Call this to fetch randomness to buffer when CCC is used as library
 * for "kernel code" (same address space calls).
 *
 * This manages the mutex and selects the proper engine internally.
 *
 * NOTE: This function uses the RNG1 mutex locks directly (does not use the
 * HW_MUTEX_TAKE_ENGINE/HW_MUTEX_RELEASE_ENGINE macros) so that this function
 * can be always called no matter what other mutexes are held by the caller.
 *
 * This requires that the RNG1 mutex is not granted to anyone if it is lwrrently
 * held (by anyone)! I think the HW works this way => please let me know if
 * you disagree!
 *
 * If this is not true, then operations that already hold some mutex would be
 * required to call DRNG locked function protected by the same mutex: SE => SE0_DRNG,
 * PKA1 => TRNG and RNG1 could be only called when no mutexes are held (as the
 * macros lock the "CCC system SW mutex", they prevent such cross-mutex calls!)
 */
status_t rng1_generate_drng(uint8_t *dbuf,
			    uint32_t dbuf_len)
{
	status_t ret = NO_ERROR;
	const engine_t *engine = NULL;
	bool mutex_locked = false;
	bool init_done = false;

	LTRACEF("entry\n");

	ret = ccc_select_device_engine(SE_CDEV_RNG1, &engine, CCC_ENGINE_CLASS_DRNG);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to select RNG1 engine\n"));

	/* Must not use the HW_MUTEX_TAKE_ENGINE/HW_MUTEX_RELEASE_ENGINE
	 * macros here!
	 */
	SYSTEM_CRYPTO_INIT(engine);
	init_done = true;

	ret = rng1_acquire_mutex(engine);
	CCC_ERROR_CHECK(ret);

	mutex_locked = true;

	ret = rng1_generate_locked(engine, dbuf, dbuf_len);
	CCC_ERROR_CHECK(ret,
			LTRACEF("RNG1 failed to generate DRNG\n"));
fail:
	if (BOOL_IS_TRUE(mutex_locked)) {
		rng1_release_mutex(engine);
	}

	if (BOOL_IS_TRUE(init_done)) {
		SYSTEM_CRYPTO_DEINIT(engine);
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_RNG1_DRNG */
