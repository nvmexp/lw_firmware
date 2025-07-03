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

/* All SE engine HW related code should be in this file.
 * If not => move it here and move other code out of here (TODO).
 */

/* NOTE about key slots.  As a convention we should assume that one
 * out of the 4 rsa keyslots is reserved for secure code (e.g. this
 * driver) and this reg shyould then be set TZ access only ( which is
 * a sticky to reset setting). Otherwise the SE driver can not do any
 * RSA operations with the SE PKA0 unit.
 *
 * As a convention (as dislwssed and suggested by me ;-) I will take
 * the last RSA key slot (slot 3) and will make it TZ only at some
 * point of time before releasing this driver...
 *
 * Same thing for AES key slots => I need at least one keyslot for
 * this code, but I do not know which number I can start to use =>
 * more dislwssions required. Now using AES key slot 2 without
 * agreements of any kind for testing (but hey, it seems to
 * work!).
 *
 * What ever slot is reserved for this it will most likely be
 * the only keyslot this driver can use to write keys. Existing key
 * use will need some authorization mechanism (as dislwssed the
 * dynloader adds simple manifest authorizations) but theese are not
 * yet merged. Lwrrently any client can use any keyslot (which is not
 * really that fine, even though all clients are secure).
 *
 * In case I do the keystore using ciphered keys => then it might be
 * required to allocate one more AES keyslot for this purpose.
 */

/* XXX TODO set "TZ use only" to any keyslot we set in this code before setting it...
 */

/**
 * @file   tegra_se_gen.c
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2015
 *
 * @brief  Engine layer code for operations that are shared between
 * T18X/T19X and T23X
 */

#include <crypto_system_config.h>

#include <ccc_lwrm_drf.h>
#include <arse0.h>

#include <include/tegra_se.h>
#include <tegra_se_gen.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#ifndef SE0_MUTEX_WATCHDOG_TIMEOUT_VALUE
#define SE0_MUTEX_WATCHDOG_TIMEOUT_VALUE 0xFFFFFFFFU
#endif

#ifndef HAVE_SE_ERROR_CAPTURE
#if defined(SE0_ERROR_CAPTURE_0)
#define HAVE_SE_ERROR_CAPTURE HAVE_ERROR_CAPTURE
#else
#define HAVE_SE_ERROR_CAPTURE 0
#endif
#endif /* HAVE_SE_ERROR_CAPTURE */

#if HAVE_SE_SHA || HAVE_SE_AES || HAVE_SE_RSA || HAVE_SE_GENRND

#if TEGRA_DELAY_FUNCTION_UNDEFINED
#define SE0_MUTEX_LOCK_TIMEOUT		(40000U*200U) /* poll count, not microseconds */
#else
#define SE0_MUTEX_LOCK_TIMEOUT		40000U	/*micro seconds*/
#endif

// XXX FIXME => there is no kind of timeout used with the SE mutexes => should at least inform
// XXX   if a timeout expires and then retry? (or something?)
// XXX => FIXME
//
// XXX For now polls until the mutex is granted; does not support resetting SE if it is stuck.
//
// XXX SE reset is hard in general (needs HOST1X co-ordination in T19X/T186, no such
// XXX  code exists in host1x). CCC Trusty port supports force reset of SE engine, but
// XXX  that is not generic due to host1x issues.
//
/**
 * @brief Acquire SE0 h/w mutex (engine level)
 *
 * Lock the HW mutex guarding SE0 APB bus access. Loop until mutex
 * locked.  Set the watchdog counter for releasing the engie in case
 * the mutex is held too long.
 *
 * Timeout depends on engine clock speed: @600 MHz it is about 1.667s.
 * Default timeout value is the largest possible timeout value unless
 * overridden by sybsystem compilation flag:
 * SE0_MUTEX_WATCHDOG_TIMEOUT_VALUE).
 *
 * For systems that do not support mutex locks this call is a NOP when
 * HAVE_SE_MUTEX is defined 0.
 *
 * @param engine SE engine to use for accessing mutex registers
 *
 * @return NO_ERROR on success, error code otherwise.
 */
static status_t se0_get_mutex(const engine_t *engine)
{
	status_t ret = NO_ERROR;
#if HAVE_SE_MUTEX == 0
	(void)engine;
#else
	uint32_t se_config_reg = 0U;
	uint32_t status = SE0_MUTEX_REQUEST_RELEASE_0_RESET_VAL;
	uint32_t wd_timeout = SE0_MUTEX_WATCHDOG_TIMEOUT_VALUE;
	uint32_t i = 0U;

	MCP("start");

	if (NULL == engine) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	do {
		if (i > SE0_MUTEX_LOCK_TIMEOUT) {
			CCC_ERROR_MESSAGE("Acquire SE0 Mutex timed out\n");
			ret = SE_ERROR(ERR_TIMED_OUT);
			continue;
		}

		se_config_reg = tegra_engine_get_reg_NOTRACE(engine, SE0_MUTEX_REQUEST_RELEASE_0);
		status = LW_DRF_VAL(SE0_MUTEX, REQUEST_RELEASE, LOCK, se_config_reg);

		if (SE0_MUTEX_REQUEST_RELEASE_0_LOCK_TRUE == status) {
			break;
		}

#if TEGRA_UDELAY_VALUE_SE0_MUTEX > 0
		TEGRA_UDELAY(TEGRA_UDELAY_VALUE_SE0_MUTEX);
#endif
		i++;
	} while (NO_ERROR == ret);
	CCC_ERROR_CHECK(ret, CCC_ERROR_MESSAGE("SE0 mutex lock failed: %d\n", ret));

	/* Set the WD timeout explicitly in case some other code has
	 * changed the by default load value.
	 *
	 * CCC configured default timeout is (0xFFFFFFFF * 256) SE
	 * clock cycles (max value). This is about 1.6 seconds @600
	 * MHz.
	 */
	tegra_engine_set_reg(engine, SE0_MUTEX_WATCHDOG_COUNTER_0,
			     LW_DRF_VAL(SE0_MUTEX, WATCHDOG_COUNTER, VAL, wd_timeout));
	MCP("done");
fail:
#endif /* HAVE_SE_MUTEX == 0 */
	return ret;
}

/**
 * @brief Release SE0 h/w mutex (engine level)
 *
 * For systems that do not support mutex locks this call is a NOP when
 * HAVE_SE_MUTEX is defined 0.
 *
 * @param engine SE engine to use for accessing mutex registers
 */
static void se0_release_mutex(const engine_t *engine)
{
#if HAVE_SE_MUTEX == 0
	(void)engine;
#else
	MCP("start");
	tegra_engine_set_reg(engine,
			     SE0_MUTEX_REQUEST_RELEASE_0,
			     SE0_MUTEX_REQUEST_RELEASE_0_LOCK_TRUE);
	MCP("done");
#endif /* HAVE_SE_MUTEX == 0 */
}

static uint32_t hw_mstate;

#if SE_RD_DEBUG
uint32_t se_hw_mstate_get(void)
{
	return hw_mstate;
}
#endif

status_t tegra_se0_mutex_lock(const struct engine_s *engine)
{
	status_t ret = NO_ERROR;

	if (0U != hw_mstate) {
		LTRACEF("Mutex hw_mstate already locked before take: %u\n", hw_mstate);
		DEBUG_ASSERT(0U == hw_mstate);
	}

	ret = se0_get_mutex(engine);
	CCC_ERROR_CHECK(ret);

	hw_mstate++;

	if (1U != hw_mstate) {
		LTRACEF("Mutex hw_mstate lock count not one after take: %u, ret %d\n",
			hw_mstate, ret);
		DEBUG_ASSERT(1U == hw_mstate);
	}

fail:
	return ret;
}

void tegra_se0_mutex_release(const struct engine_s *engine)
{
	if (1U != hw_mstate) {
		LTRACEF("Mutex hw_mstate not one before release: %u\n", hw_mstate);
		DEBUG_ASSERT(1U == hw_mstate);
	}

	se0_release_mutex(engine);
	hw_mstate--;

	if (0U != hw_mstate) {
		LTRACEF("Mutex hw_mstate lock count not zero after release: %u\n", hw_mstate);
		DEBUG_ASSERT(0U == hw_mstate);
	}
}

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
void show_se_gen_op_entry_perf(const char *oname, uint32_t elapsed)
{
	CCC_ERROR_MESSAGE("SE engine [%s]: %u usec\n",
			  ((NULL == oname) ? "" : oname), elapsed);
}
#endif

/*****************************/

static uint32_t se_engine_max_timeout = SE_MAX_TIMEOUT;

#if HAVE_NO_STATIC_DATA_INITIALIZER
void ccc_static_init_se_values(void)
{
	se_engine_max_timeout = SE_MAX_TIMEOUT;
}
#endif /* HAVE_NO_STATIC_DATA_INITIALIZER */

#if HAVE_SE_ENGINE_TIMEOUT
uint32_t get_se_engine_max_timeout(void)
{
	return se_engine_max_timeout;
}

status_t se_engine_set_max_timeout(uint32_t max_timeout)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	if (max_timeout <= SE_MIN_TIMEOUT) {
		/* Use 0 value argument to see current value in log */
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					 CCC_ERROR_MESSAGE("SE poll count %u too small (using: %u)\n",
							   max_timeout, se_engine_max_timeout));
	}

	if (max_timeout == 0xFFFFFFFFU) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	se_engine_max_timeout = max_timeout;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_ENGINE_TIMEOUT */

/*****************************/

#if CCC_CLEAR_GSCID_REG
static void set_gscid(const engine_t *engine)
{
	uint32_t gscid_reg = 0U;

	switch (engine->e_id) {
	case CCC_ENGINE_SE0_AES0:
		gscid_reg = SE0_AES0_GSCID_0;
		break;
	case CCC_ENGINE_SE0_AES1:
		gscid_reg = SE0_AES1_GSCID_0;
		break;
	case CCC_ENGINE_SE0_SHA:
		gscid_reg = SE0_SHA_GSCID_0;
		break;
#if HAVE_SE_RSA
	case CCC_ENGINE_SE0_PKA0:
		gscid_reg = SE0_RSA_GSCID_0;
		break;
#endif
#if HAVE_SE1
		/* NOTE: The are still SE0_* versions
		 *
		 * Now just assuming the offset is the same, SE1 engine handling
		 * will map these to correct base (tegra_cdev.h)
		 */
	case CCC_ENGINE_SE1_AES0:
		gscid_reg = SE0_AES0_GSCID_0;
		break;
	case CCC_ENGINE_SE1_AES1:
		gscid_reg = SE0_AES1_GSCID_0;
		break;
	case CCC_ENGINE_SE1_SHA:
		gscid_reg = SE0_SHA_GSCID_0;
		break;
#if HAVE_SE_RSA
	case CCC_ENGINE_SE1_PKA0:
		gscid_reg = SE0_RSA_GSCID_0;
		break;
#endif
#endif /* HAVE_SE1 */

	default:
		/* not possible */
		break;
	}

	tegra_engine_set_reg(engine, gscid_reg, 0x0U);
}
#endif /* CCC_CLEAR_GSCID_REG */

static inline void se_device_clear_error_capture(const se_cdev_t *dev)
{
	(void)dev;
#if defined(SE0_ERROR_CAPTURE_0)
	if (NULL != dev) {
		ccc_device_set_reg(dev, SE0_ERROR_CAPTURE_0,
				   LW_DRF_DEF(SE0, ERROR_CAPTURE, ERROR_BIT, SET));
	}
#endif
}

#if HAVE_SE_ERROR_CAPTURE
/* Get and reset the SE0_ERROR_CAPTURE register
 * Note that this is not a per engine register, it is per device.
 */
static status_t se_device_check_error_capture(const se_cdev_t *dev,
					      uint32_t *error_info_p,
					      bool clear)
{
	status_t ret = NO_ERROR;
	uint32_t error_info = 0U;

	LTRACEF("entry\n");

	if (NULL != dev) {
		uint32_t reg_val = ccc_device_get_reg(dev, SE0_ERROR_CAPTURE_0);

		if (0U != LW_DRF_VAL(SE0, ERROR_CAPTURE, ERROR_BIT, reg_val)) {
			if (BOOL_IS_TRUE(clear)) {
				se_device_clear_error_capture(dev);
			}

			error_info = LW_DRF_VAL(SE0, ERROR_CAPTURE, ERROR_INFO, reg_val);
			ret = SE_ERROR(ERR_BAD_STATE);

			CCC_ERROR_MESSAGE("SE abnormal cond, error info: 0x%x\n",
					  error_info);
		}
	}

	if (NULL != error_info_p) {
		*error_info_p = error_info;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_ERROR_CAPTURE */

status_t tegra_start_se0_operation_locked(const engine_t *engine,
					  uint32_t se_op_reg_preset)
{
	status_t ret = NO_ERROR;
	uint32_t se_op_reg = se_op_reg_preset;
	uint32_t op = 0U;

	DEBUG_ASSERT_HW_MUTEX;

	LTRACEF("entry [op preset: 0x%x]\n", se_op_reg_preset);

	se_device_clear_error_capture(engine->e_dev);

	se_op_reg |= SE_UNIT_OPERATION_PKT_LASTBUF_FIELD;

	se_op_reg |= (SE_UNIT_OPERATION_PKT_OP_START <<
		      SE_UNIT_OPERATION_PKT_OP_SHIFT);

	switch (engine->e_id) {
	case CCC_ENGINE_SE0_AES0:
		op = SE0_AES0_OPERATION_0;
		break;
	case CCC_ENGINE_SE0_AES1:
		op = SE0_AES1_OPERATION_0;
		break;
	case CCC_ENGINE_SE0_SHA:
		op = SE0_SHA_OPERATION_0;
		break;
#if HAVE_SE_RSA
	case CCC_ENGINE_SE0_PKA0:
		op = SE0_RSA_OPERATION_0;
		break;
#endif
#if HAVE_SE1
		/* NOTE: The are still SE0_* versions
		 *
		 * Now just assuming the offset is the same, SE1 engine handling
		 * will map these to correct base (tegra_cdev.h)
		 */
	case CCC_ENGINE_SE1_AES0:
		op = SE0_AES0_OPERATION_0;
		break;
	case CCC_ENGINE_SE1_AES1:
		op = SE0_AES1_OPERATION_0;
		break;
	case CCC_ENGINE_SE1_SHA:
		op = SE0_SHA_OPERATION_0;
		break;
#if HAVE_SE_RSA
	case CCC_ENGINE_SE1_PKA0:
		op = SE0_RSA_OPERATION_0;
		break;
#endif
#endif /* HAVE_SE1 */

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

#if CCC_CLEAR_GSCID_REG
	set_gscid(engine);
#endif /* CCC_CLEAR_GSCID_REG */

	/* Added a compiler fence before SE op starts */
	CF;
	tegra_engine_set_reg(engine, op, se_op_reg);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/****************************/

#if HAVE_SE_ASYNC
status_t se_async_check_engine_free(const engine_t *engine,
				    bool *is_idle_p)
{
	status_t ret = NO_ERROR;
	uint32_t se_config_offset = 0U;
	uint32_t status = 0U;
	uint32_t se_config_reg = 0U;

	if ((NULL == engine) || (NULL == is_idle_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*is_idle_p = false;

	switch (engine->e_id) {
	case CCC_ENGINE_SE0_AES0:
		se_config_offset = SE0_AES0_STATUS_0;
		break;
	case CCC_ENGINE_SE0_AES1:
		se_config_offset = SE0_AES1_STATUS_0;
		break;
	case CCC_ENGINE_SE0_SHA:
		se_config_offset = SE0_SHA_STATUS_0;
		break;
#if HAVE_SE_RSA
	case CCC_ENGINE_SE0_PKA0:
		se_config_offset = SE0_RSA_STATUS_0;
		break;
#endif
#if HAVE_SE1
	case CCC_ENGINE_SE1_AES0:
		se_config_offset = SE0_AES0_STATUS_0;
		break;
	case CCC_ENGINE_SE1_AES1:
		se_config_offset = SE0_AES1_STATUS_0;
		break;
	case CCC_ENGINE_SE1_SHA:
		se_config_offset = SE0_SHA_STATUS_0;
		break;
#if HAVE_SE_RSA
	case CCC_ENGINE_SE1_PKA0:
		se_config_offset = SE0_RSA_STATUS_0;
		break;
#endif
#endif /* HAVE_SE1 */

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	se_config_reg = tegra_engine_get_reg_NOTRACE(engine, se_config_offset);

	/* SE0_*_STATUS_0_STATE_* fields are the same for each STATUS register,
	 * using AES0 status' state to extract the status field value.
	 */
	status = LW_DRF_VAL(SE0_AES0, STATUS, STATE, se_config_reg);

	if (SE0_AES0_STATUS_0_STATE_IDLE == status) {
		*is_idle_p = true;
	} else {
#if HAVE_SE_MUTEX
		/* Reset the SE MUTEX WATCHDOG TIMER when checking if the engine is IDLE
		 */
		tegra_engine_set_reg(engine, SE0_MUTEX_WATCHDOG_COUNTER_0,
				     LW_DRF_VAL(SE0_MUTEX, WATCHDOG_COUNTER, VAL,
						SE0_MUTEX_WATCHDOG_TIMEOUT_VALUE));
#endif /* HAVE_SE_MUTEX */
	}

fail:
	return ret;
}
#endif /* HAVE_SE_ASYNC */

#if HAVE_SOC_FEATURE_KAC
/* This is SE INT_STATUS bit error value checker for
 * HW error bits.
 */
#define CCC_NUM_KAC_INT_STATUS_ERR_STEPS 3U

static status_t se_kac_int_status_check(const engine_t *engine,
					status_t *ret2_p)
{
	status_t ret = NO_ERROR;
	uint32_t steps = 0U;
	uint32_t mask = 0U;
	uint32_t is = 0xFFFFFFFFU;

	FI_BARRIER(u32, is);

	is = tegra_engine_get_reg(engine, SE0_INT_STATUS_0);
	steps++;

	/* CCC does not handle the CORRECTED ECC error bit.
	 */
	if (LW_DRF_VAL(SE0, INT_STATUS, KSLT_ECC_UNCORRECTED_ERROR, is) != 0U) {
		ret = ERR_FAULT;
		mask = LW_FLD_SET_DRF_DEF(SE0, INT_STATUS,
					  KSLT_ECC_UNCORRECTED_ERROR,
					  SW_CLEAR, mask);
	}
	steps++;
	FI_BARRIER(u32, steps);

	/* Avoid cert-c overflow */
	if (steps > 2U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	if (LW_DRF_VAL(SE0, INT_STATUS, DCLS_ERROR, is) != 0U) {
		ret = ERR_FAULT;
		mask = LW_FLD_SET_DRF_DEF(SE0, INT_STATUS, DCLS_ERROR, SW_CLEAR, mask);
	}
	steps++;

	if (0U != mask) {
		tegra_engine_set_reg(engine, SE0_INT_STATUS_0, mask);
	}

	CCC_ERROR_CHECK(ret);

	*ret2_p = NO_ERROR;

	FI_BARRIER(u32, steps);
	FI_BARRIER(status, ret);
fail:
	if ((NO_ERROR == ret) && (CCC_NUM_KAC_INT_STATUS_ERR_STEPS != steps)) {
		ret = ERR_BAD_STATE;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SOC_FEATURE_KAC */

static status_t intern_poll_engine_free(const engine_t *engine,
					uint32_t se_config_offset,
					status_t *ret2_p)
{
	status_t ret = NO_ERROR;
	uint32_t timeout = 0U;
	uint32_t status = 0U;

	LTRACEF("entry\n");

#if TEGRA_UDELAY_VALUE_SE0_OP_INITIAL > 0
	TEGRA_UDELAY(TEGRA_UDELAY_VALUE_SE0_OP_INITIAL);
#endif

	do {
		uint32_t se_config_reg = tegra_engine_get_reg_NOTRACE(engine, se_config_offset);

		/* SE0_*_STATUS_0_STATE_* are the same for each STATUS register,
		 * using SE0_AES0 status state to extract the status field value.
		 */
		status = LW_DRF_VAL(SE0_AES0, STATUS, STATE, se_config_reg);

		timeout++;

		if (SE0_AES0_STATUS_0_STATE_IDLE == status) {
			break;
		}

#if TEGRA_UDELAY_VALUE_SE0_OP > 0
		TEGRA_UDELAY(TEGRA_UDELAY_VALUE_SE0_OP);
#endif
	} while (timeout <= se_engine_max_timeout);

#if HAVE_SOC_FEATURE_KAC
	ret = se_kac_int_status_check(engine, ret2_p);
	CCC_ERROR_CHECK(ret);
#else
	*ret2_p = NO_ERROR;
#endif

	if (timeout > se_engine_max_timeout) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("Engine busy state too many times %u (status 0x%x)\n",
						       timeout, status));
	}
fail:
	LTRACEF("exit: engine[%s (%u)]: wait free loop count %u (status 0x%x), ret %d\n",
		engine->e_name, engine->e_id, timeout, status, ret);

	return ret;
}

/*
 * T23x: Also checks HW INT_STATUS bits for DCLS and KSLT ECC
 * errors on operation completion. If set, returns
 * ERR_FAULT and clears the bits. These bits are also
 * monitored (out of band) by SCE and FSI.
 */
status_t se_wait_engine_free(const engine_t *engine)
{
	status_t ret = NO_ERROR;
	uint32_t se_config_offset = 0U;
	status_t ret2 = ERR_BAD_STATE;

	LTRACEF("entry\n");

	switch (engine->e_id) {
	case CCC_ENGINE_SE0_AES0:
		se_config_offset = SE0_AES0_STATUS_0;
		break;
	case CCC_ENGINE_SE0_AES1:
		se_config_offset = SE0_AES1_STATUS_0;
		break;
	case CCC_ENGINE_SE0_SHA:
		se_config_offset = SE0_SHA_STATUS_0;
		break;
#if HAVE_SE_RSA
	case CCC_ENGINE_SE0_PKA0:
		se_config_offset = SE0_RSA_STATUS_0;
		break;
#endif
#if HAVE_SE1
	case CCC_ENGINE_SE1_AES0:
		se_config_offset = SE0_AES0_STATUS_0;
		break;
	case CCC_ENGINE_SE1_AES1:
		se_config_offset = SE0_AES1_STATUS_0;
		break;
	case CCC_ENGINE_SE1_SHA:
		se_config_offset = SE0_SHA_STATUS_0;
		break;
#if HAVE_SE_RSA
	case CCC_ENGINE_SE1_PKA0:
		se_config_offset = SE0_RSA_STATUS_0;
		break;
#endif
#endif
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	/* Poll until not BUSY */
	ret = intern_poll_engine_free(engine, se_config_offset, &ret2);
	CCC_ERROR_CHECK(ret);
fail:
	if (NO_ERROR == ret) {
		ret = ret2;
	}
	return ret;
}

static status_t se_check_hw_err_status(const engine_t *engine,
				       uint32_t err_reg,
				       status_t *ret2_p,
				       const char *str)
{
	status_t ret = ERR_BAD_STATE;
	uint32_t val = 0xFFFFFFFFU;

	LTRACEF("entry\n");

	(void)str;

	if (0U == err_reg) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* Check err_status register to make sure engine state is ok */
	val = tegra_engine_get_reg(engine, err_reg);
	*ret2_p = NO_ERROR;

	if (val > 0U) {
		/* SW_CLEAR any error state after reporting it */
		tegra_engine_set_reg(engine, err_reg, val);

		CCC_ERROR_WITH_ECODE(ERR_NOT_ALLOWED,
				     CCC_ERROR_MESSAGE("engine %s(%u) %s: %s err_status register 0x%x nonzero (value 0x%x)\n",
						       engine->e_name, engine->e_id,
						       ((NULL == str) ? "" : str),
						       engine->e_dev->cd_name, err_reg, val));
	}

#if HAVE_SE_ERROR_CAPTURE
	ret = se_device_check_error_capture(engine->e_dev, error_info, false);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("device %s(%u) %s: error capture 0x%x\n",
					  engine->e_dev->cd_name, engine->e_dev->cd_id,
					  ((NULL == str) ? "" : str),
					  val));
#endif /* HAVE_SE_ERROR_CAPTURE */

	ret = NO_ERROR;
fail:
	se_device_clear_error_capture(engine->e_dev);

	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_engine_get_err_status(const engine_t *engine, const char *str)
{
	status_t ret = NO_ERROR;
	uint32_t err_reg = 0U;
	status_t ret2 = ERR_BAD_STATE;

	LTRACEF("entry\n");

	switch(engine->e_id) {
	case CCC_ENGINE_SE0_AES0:
		err_reg = SE0_AES0_ERR_STATUS_0;
		break;
	case CCC_ENGINE_SE0_AES1:
		err_reg = SE0_AES1_ERR_STATUS_0;
		break;
	case CCC_ENGINE_SE0_SHA:
		err_reg = SE0_SHA_ERR_STATUS_0;
		break;
#if HAVE_SE_RSA
	case CCC_ENGINE_SE0_PKA0:
		err_reg = SE0_RSA_ERR_STATUS_0;
		break;
#endif
#if HAVE_SE1
	case CCC_ENGINE_SE1_AES0:
		err_reg = SE0_AES0_ERR_STATUS_0;
		break;
	case CCC_ENGINE_SE1_AES1:
		err_reg = SE0_AES1_ERR_STATUS_0;
		break;
	case CCC_ENGINE_SE1_PKA0:
		err_reg = SE0_RSA_ERR_STATUS_0;
		break;
	case CCC_ENGINE_SE1_SHA:
		err_reg = SE0_SHA_ERR_STATUS_0;
		break;
#endif
	default:
		CCC_ERROR_MESSAGE("%s: Unsupported SE engine %u\n",
				  ((NULL == str) ? "" : str), engine->e_id);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	ret = se_check_hw_err_status(engine, err_reg, &ret2, str);
	CCC_ERROR_CHECK(ret);

	FI_BARRIER(status, ret2);
fail:
	if (NO_ERROR == ret) {
		ret = ret2;
	}
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#endif /*  HAVE_SE_SHA || HAVE_SE_AES || HAVE_SE_RSA || HAVE_SE_GENRND */

#if HAVE_SE_AES

#if HAVE_AES_CTR
/* Modulo 2^128 bit big endian linear counter increment.
 */
static status_t se_update_aes_ctr_linear_counter(struct se_engine_aes_context *econtext,
						 uint32_t num_blocks)
{
	status_t ret = NO_ERROR;
	uint64_t ctr64 = 0UL;
	uint64_t val64 = 0UL;
	bool buf_BE = ((econtext->aes_flags & AES_FLAGS_CTR_MODE_BIG_ENDIAN) != 0U);

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(buf_BE)) {
		ctr64 = se_util_buf_to_u64((const uint8_t *)&econtext->ctr.counter[2U], true);
	} else {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
	}

	CCC_SAFE_MULT_U64(val64, (uint64_t)num_blocks, (uint64_t)econtext->ctr.increment);

	val64 = ctr64 + val64;
	if (val64 < ctr64) {
		/* overflow on low 64 bit value, add carry to high longword */
		ctr64 = se_util_buf_to_u64((const uint8_t *)&econtext->ctr.counter[0U], true);

		/* This does not trap the overflow, instead returns an
		 * error. The overflow error is checked and returned
		 * but ignored here, because this is a 2^128 bit
		 * modular counter.
		 */
		(void)CCC_ADD_U64(ctr64, ctr64, (uint64_t)1U);

		/* Save high longword after carry addition to memory in big endian */
		se_util_u64_to_buf(ctr64, (uint8_t *)&econtext->ctr.counter[0U], true);
	}

	/* Save low longword increment result to memory in big endian */
	se_util_u64_to_buf(val64, (uint8_t *)&econtext->ctr.counter[2U], true);

	LOG_HEXBUF("AES CTR updated", (const uint8_t *)econtext->ctr.counter, SE_AES_BLOCK_LENGTH);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_AES_CTR */

#if HAVE_AES_CCM
static status_t se_update_aes_ccm_linear_counter(struct se_engine_aes_context *econtext,
						 uint32_t num_blocks)
{
	status_t ret = NO_ERROR;
	uint64_t ctr64 = 0UL;
	bool buf_BE = ((econtext->aes_flags & AES_FLAGS_CTR_MODE_BIG_ENDIAN) != 0U);

	/* AES counter format => "flags | nonce | counter"
	 */
	uint32_t ctr_bytes = 0U;

	LTRACEF("entry\n");

	/* the "flags" field is 1 byte long and
	 * flags_len + nonce_len + counter_len == 16 (aes counter length)
	 */
	if (econtext->ccm.nonce_len >= SE_AES_BLOCK_LENGTH) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}
	ctr_bytes = (SE_AES_BLOCK_LENGTH - econtext->ccm.nonce_len - 1U);

	if (ctr_bytes > 8U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("AES-CCM nonce len invalid: %u\n",
					     econtext->ccm.nonce_len));
	}

	if (BOOL_IS_TRUE(buf_BE)) {
		ctr64 = se_util_buf_to_u64((const uint8_t *)&econtext->ccm.counter[2U], true);
	} else {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("CCM doesn't support little endian counters\n"));
	}

	LTRACEF("AES-CCM: original counter value 0x%llx\n", (unsigned long long)ctr64);

	CCC_SAFE_ADD_U64(ctr64, ctr64, (uint64_t)num_blocks);

	se_util_u64_to_buf(ctr64, (uint8_t *)&econtext->ccm.counter[2U], true);

	LTRACEF("AES-CCM: next counter value 0x%llx\n", (unsigned long long)ctr64);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_AES_CCM */

#if CCC_WITH_AES_GCM
static status_t se_update_aes_gcm_linear_counter(struct se_engine_aes_context *econtext,
						 uint32_t num_blocks)
{
	status_t ret = NO_ERROR;
	uint32_t ctr32 = 0U;
	bool buf_BE = ((econtext->aes_flags & AES_FLAGS_CTR_MODE_BIG_ENDIAN) != 0U);

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(buf_BE)) {
		/* BE counter has counter in bytes 12..15 in big endian order */
		ctr32 = se_util_swap_word_endian(econtext->gcm.counter[ 3 ]);
	} else {
		/* LE counter has counter in bytes 0..3 in little endian order */
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
	}

	/* In practice this will never overflow when counter starts from
	 * 0 (as it does in GCM spec). (2^32-1)*16 == about 68 Gigabytes
	 */
	CCC_SAFE_ADD_U32(ctr32, ctr32, num_blocks);

	econtext->gcm.counter[ 3 ] = se_util_swap_word_endian(ctr32);

	LOG_HEXBUF("GCM updated counter", (uint8_t *)econtext->gcm.counter, SE_AES_BLOCK_LENGTH);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_AES_GCM */

#if HAVE_AES_CCM || HAVE_AES_CTR || HAVE_AES_GCM
/* In AES CTR mode, update the counter after AES =>
 * ctr.counter += num_blocks * ctr.increment
 *
 * Similar for AES_GCM mode, for econtext->gcm.counter
 *
 * AES_XTS mode does not support counter updates, since the tweak is modified
 * for each call (tweak is written to counter registers in XTS mode).
 *
 * CTR buffer big endian by default (AES_FLAGS_CTR_MODE_BIG_ENDIAN is set).
 */
status_t se_update_linear_counter(struct se_engine_aes_context *econtext,
				  uint32_t num_blocks)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (!TE_AES_MODE_COUNTER(econtext->algorithm)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("AES counter update for non-counter mode: %u\n",
					     econtext->algorithm));
	}

#if HAVE_AES_CTR
	if (TE_ALG_AES_CTR == econtext->algorithm) {

		ret = se_update_aes_ctr_linear_counter(econtext, num_blocks);
		CCC_ERROR_CHECK(ret);
	}
#endif

#if HAVE_AES_CCM
	if (TE_ALG_AES_CCM == econtext->algorithm) {
		ret = se_update_aes_ccm_linear_counter(econtext, num_blocks);
		CCC_ERROR_CHECK(ret);
	}
#endif /* HAVE_AES_CCM */

#if CCC_WITH_AES_GCM
	if (TE_ALG_AES_GCM == econtext->algorithm) {
		ret = se_update_aes_gcm_linear_counter(econtext, num_blocks);
		CCC_ERROR_CHECK(ret);
	}
#endif /* CCC_WITH_AES_GCM */

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_AES_CCM || HAVE_AES_CTR || HAVE_AES_GCM */

#if HAVE_AES_COUNTER_OVERFLOW_CHECK
#if HAVE_AES_CTR
/* When AES-CTR checks counter overflows, it only supports 64 bit counter field
 * for overflow checks...
 *
 * XXX FIXME: This restriction is rather odd.
 */
static status_t aes_check_aes_ctr_counter_overflow(const struct se_engine_aes_context *econtext,
						   uint32_t num_blocks)
{
	status_t ret = NO_ERROR;
	uint64_t ctr64_le = (uint64_t)0UL;
	uint64_t val64 = (uint64_t)0UL;
	bool buf_BE = ((econtext->aes_flags & AES_FLAGS_CTR_MODE_BIG_ENDIAN) != 0U);

	LTRACEF("entry\n");

	/* CTR implementation only supports 8 byte IV values (8 byte counter)
	 * with overflow checks. Without overflow checks the counter is treated as
	 * mod 2^128 counter.
	 *
	 * XXX Admittedly this is odd.
	 */
	if (BOOL_IS_TRUE(buf_BE)) {
		ctr64_le = se_util_buf_to_u64((const uint8_t *)&econtext->ctr.counter[2U], true);
	} else {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
	}

	LTRACEF("AES-CTR: original 64 bit counter value %llu\n",
		(unsigned long long)ctr64_le);

	CCC_SAFE_MULT_U64(val64, (uint64_t)num_blocks, (uint64_t)econtext->ctr.increment);

	val64 = ctr64_le + val64;
	if (ctr64_le > val64) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG,
				     LTRACEF("AES-CTR 64 bit counter overflow detected with %u blocks\n",
					     num_blocks));
	}

	LTRACEF("AES-CTR: next counter value %llu\n", (unsigned long long)val64);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_AES_CTR */

#if HAVE_AES_CCM
static status_t aes_check_aes_ccm_counter_overflow(const struct se_engine_aes_context *econtext,
						   uint32_t num_blocks)
{
	status_t ret = NO_ERROR;
	uint64_t ctr64_le = 0UL;

	/* AES counter format => "flags | nonce | counter"
	 */
	uint32_t ctr_bytes = 0U;
	uint64_t ctr64_le_orig = 0UL;
	uint64_t cmask64  = ~0UL;
	bool     overflow = false;
	bool buf_BE = ((econtext->aes_flags & AES_FLAGS_CTR_MODE_BIG_ENDIAN) != 0U);

	LTRACEF("entry\n");

	/* the "flags" field is 1 byte long and
	 * flags_len + nonce_len + counter_len == 16 (aes counter length)
	 */
	if (econtext->ccm.nonce_len >= (SE_AES_BLOCK_LENGTH - 1U)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}
	ctr_bytes = (SE_AES_BLOCK_LENGTH - econtext->ccm.nonce_len - 1U);

	if (ctr_bytes > 8U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("AES-CCM nonce len invalid: %u\n",
					     econtext->ccm.nonce_len));
	}

	if (BOOL_IS_TRUE(buf_BE)) {
		ctr64_le = se_util_buf_to_u64((const uint8_t *)&econtext->ccm.counter[2U], true);
	} else {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("CCM doesn't support little endian counters\n"));
	}

	cmask64  = cmask64 >> (8U - ctr_bytes);
	ctr64_le = ctr64_le & cmask64;
	ctr64_le_orig = ctr64_le;

	LTRACEF("AES-CCM: original counter value %llu\n", (unsigned long long)ctr64_le);

	ctr64_le = ctr64_le + (uint64_t)num_blocks;
	overflow = ((ctr64_le & ~cmask64) != 0ULL);
	overflow = overflow || (ctr64_le < ctr64_le_orig);

	if (BOOL_IS_TRUE(overflow)) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG,
				     LTRACEF("AES-CCM %u bit block counter overflow (%u blocks)\n",
					     (ctr_bytes * 8U), num_blocks));
	}

	LTRACEF("AES-CCM: next counter value %llu\n", (unsigned long long)ctr64_le);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_AES_CCM */

#if CCC_WITH_AES_GCM
static status_t aes_check_aes_gcm_counter_overflow(const struct se_engine_aes_context *econtext,
						   uint32_t num_blocks)
{
	status_t ret = NO_ERROR;
	uint32_t ctr32 = 0U;
	bool buf_BE  = ((econtext->aes_flags & AES_FLAGS_CTR_MODE_BIG_ENDIAN) != 0U);

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(buf_BE)) {
		ctr32 = se_util_swap_word_endian(econtext->gcm.counter[ 3 ]);
	} else {
		/* LE counter has counter in bytes 0..3 in little endian order */
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
	}

	/* In practice this will never overflow when counter starts from
	 * 0 (as it does in GCM spec). (2^32-1)*16 == about 68 Gigabytes
	 */
	CCC_SAFE_ADD_U32(ctr32, ctr32, num_blocks);

	LTRACEF("AES-GCM: next counter value %u\n", ctr32);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_AES_GCM */

/*
 * With STD counter values overflow probably never oclwrs. But in case
 * client passes large counter init values it might overflow the 64
 * bit field and change the nonce. In such case, return an error
 * instead.
 *
 * TODO: Overflow AES-CTR detection supports only 64 bit counter field. Could
 * support other lengths if required, but the api does not now contain
 * counter field length.
 */
static status_t aes_check_counter_overflow(const struct se_engine_aes_context *econtext,
					   uint32_t num_blocks)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (!TE_AES_MODE_COUNTER(econtext->algorithm)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("AES counter check for non-counter algorithm: %u\n",
					     econtext->algorithm));
	}

#if HAVE_AES_CTR
	if (TE_ALG_AES_CTR == econtext->algorithm) {

		ret = aes_check_aes_ctr_counter_overflow(econtext, num_blocks);
		CCC_ERROR_CHECK(ret);
	}
#endif

#if HAVE_AES_CCM
	if (TE_ALG_AES_CCM == econtext->algorithm) {

		ret = aes_check_aes_ccm_counter_overflow(econtext, num_blocks);
		CCC_ERROR_CHECK(ret);
	}
#endif /* HAVE_AES_CCM */

#if CCC_WITH_AES_GCM
	if (TE_ALG_AES_GCM == econtext->algorithm) {
		ret = aes_check_aes_gcm_counter_overflow(econtext, num_blocks);
		CCC_ERROR_CHECK(ret);
	}
#endif /* CCC_WITH_AES_GCM */

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t do_aes_counter_overflow_check(const async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(ac->update_ctr)) {
		ret = aes_check_counter_overflow(ac->econtext, ac->num_blocks);
		if (ERR_TOO_BIG == ret) {
			CCC_ERROR_WITH_ECODE(ERR_TOO_BIG,
					     LTRACEF("AES algorithm 0x%x counter error (overflow?) within %u blocks (%d)\n",
						     ac->econtext->algorithm, ac->num_blocks, ret));
		}
		CCC_ERROR_CHECK(ret);
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_AES_COUNTER_OVERFLOW_CHECK */

/* NULL cvalue writes zeros to linear counter register
 */
status_t se_write_linear_counter(const engine_t *engine,
				 const uint32_t *cvalue)
{
	status_t ret = NO_ERROR;
	uint32_t inx = 0U;

	LTRACEF("entry\n");

	if (NULL != cvalue) {
		LOG_HEXBUF("linear counter:", cvalue, SE_AES_BLOCK_LENGTH);
	}

	for (; inx < 4U; inx++) {
		tegra_engine_set_reg(engine,
				     SE0_AES0_CRYPTO_LINEAR_CTR_0 + (inx * 4U),
				     ((NULL == cvalue) ? 0U : cvalue[inx]));
	}

	FI_BARRIER(u32, inx);

	if (4U != inx) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t engine_aes_op_mode_write_linear_counter(
	const struct se_engine_aes_context *econtext,
	te_crypto_algo_t algorithm)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

#if HAVE_AES_CTR
	/* With AES-CTR mode set the <NONCE || COUNTER> 16
	 * byte preset value.
	 *
	 * NOTE: SW must keep track of the counter value OOB if
	 * the operation must be continued!
	 */
	if (TE_ALG_AES_CTR == algorithm) {
		ret = se_write_linear_counter(econtext->engine,
					      &econtext->ctr.counter[0]);
		CCC_ERROR_CHECK(ret);
	}
#endif

#if HAVE_AES_CCM
	if (TE_ALG_AES_CCM == algorithm) {
		ret = se_write_linear_counter(econtext->engine,
					      &econtext->ccm.counter[0]);
		CCC_ERROR_CHECK(ret);
	}
#endif

#if HAVE_AES_GCM
	if (TE_ALG_AES_GCM == algorithm) {
		/* AAD processing does not use/modify this counter */
		LOG_HEXBUF("GCM counter block", &econtext->gcm.counter[0], SE_AES_BLOCK_LENGTH);

		ret = se_write_linear_counter(econtext->engine,
					      &econtext->gcm.counter[0]);
		CCC_ERROR_CHECK(ret);
	}
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;

}
#endif /* HAVE_SE_AES */
