/*
 * Copyright (c) 2020-2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/*
 * LWRNG device first time initialization for ROM/MB1
 */
#include <crypto_system_config.h>

#ifndef USE_DEV_LWRNG_HEADER
//
// XXX Temporary feature: Use lw_ref_dev_lwrng.h header and
// XXX defines in it if this is defined ZERO in makefiles
//
// XXX If nonzero => use dev_lwrng.h DRF compatible header
// XXX (this is default)
//
#define USE_DEV_LWRNG_HEADER 1
#endif

/* Define this if the system running this supports RSTAT and IRSTAT
 * registers and the code should poll those like in SE IAS
 * page 184 psc_se_init_step
 */
#define POLL_LWRNG_RSTAT_STARTUP

/* XXX TODO: If there is such HW support, implement this */
#ifndef LWRNG_DISABLE_ON_KAT_ERROR
#define LWRNG_DISABLE_ON_KAT_ERROR 0
#endif

#if USE_DEV_LWRNG_HEADER
#include <ccc_lwrm_drf.h>
// include mobile header
#include <dev_lwrng.h>
#define LWRNG_REGNAME(x) x ## _0
#else
// include HW header
#include <lw_ref_dev_lwrng.h>
#define LWRNG_REGNAME(x) x
#define HW_SET_BITFIELD(val,mask,lshift) \
	(uint32_t)((uint32_t)((uint32_t)(val) & (uint32_t)(mask)) << (lshift))
#define HW_GET_BITFIELD(val,mask,rshift) \
	(uint32_t)((uint32_t)((val) >> (rshift)) & (uint32_t)(mask))
#endif

#include <tegra_lwrng_init.h>

/* If this file is compiled: Assume by default that it is also used
 * for LWRNG initialization
 */
#ifndef HAVE_LWRNG_INIT
#define HAVE_LWRNG_INIT 1
#endif

#if HAVE_LWRNG_INIT

#ifndef LWRNG_DFIFO_RETRY_COUNT
/* Retry count for reads from DFIFO in case the
 * VALID bit is zero in the word read => retry.
 */
#define LWRNG_DFIFO_RETRY_COUNT 10
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if SE_RD_DEBUG

/* Measure LWRNG reset performance if nonzero.
 * (subsystem crypto config file must also enable perf measurements)
 */
#ifndef MEASURE_PERF_LWRNG_INIT
#define MEASURE_PERF_LWRNG_INIT	0
#endif

#else

/* In production, never define these non-zero
 */
#define MEASURE_PERF_LWRNG_INIT	0

#endif /* SE_RD_DEBUG */

/* Enable lwrng soft reset code
 *
 * VDK/CMOD does not support reading register LW_LWRNG_R_CTRL1_0
 * so this can not be tested.
 */
#ifndef LWRNG_INIT_SOFT_RESET
#define LWRNG_INIT_SOFT_RESET 0
#endif

/* If zero, just do plain LWRNG init, with no DRNG generation
 *  as "warm-up" sequences.
 */
#ifndef LWRNG_INIT_WARMUP
#define LWRNG_INIT_WARMUP 0
#endif

#if MEASURE_ENGINE_PERF(MEASURE_PERF_LWRNG_INIT)

static void show_lwrng_init_perf(const char *prefix, uint32_t elapsed)
{
	if (NULL == prefix) {
		prefix = "";
	}
	CCC_ERROR_MESSAGE("%s: %u usec\n", prefix, elapsed);
}
#endif

#if TRACE_REG_OPS

/* VDK has severe output line length restrictions => need to
 * split lines or it drops output on the floor.
 */
#if CCC_SOC_FAMILY_ON(CCC_SOC_ON_VDK)
#define LOG_LWRNG_READ32(reg_s)						\
	{ LOG_ALWAYS("%u:R32[LWRNG](reg=0x%x %s)\n",			\
		     __LINE__, i__reg, (reg_s));			\
	  LOG_ALWAYS("  -> (0x%x)\n", i__data);				\
	}

#define LOG_LWRNG_WRITE32(reg_s)					\
	{								\
	   LOG_ALWAYS("%u:W32[LWRNG](r=0x%x %s)\n",			\
		      __LINE__, i__reg, (reg_s));			\
	   LOG_ALWAYS(" = (0x%x)\n", i__data);				\
	}
#else
#define LOG_LWRNG_READ32(reg_s)						\
	LOG_ALWAYS("%u:READ32[LWRNG](reg=0x%x %s) -> %u (0x%x)\n",	\
		   __LINE__, i__reg, (reg_s), i__data, i__data)

#define LOG_LWRNG_WRITE32(reg_s)					\
	LOG_ALWAYS("%u:WRITE32[LWRNG](reg=0x%x %s) = %u (0x%x)\n",	\
		   __LINE__, i__reg, (reg_s), i__data, i__data)
#endif /* SOC on VDK */

#ifndef LWRNG_SET_REG
#define LWRNG_SET_REG(baseaddr, reg, data)				\
	({ uint32_t i__reg = (reg);					\
	   uint32_t i__data = (data);					\
	   uint8_t *i__base = (baseaddr);				\
	   if (NULL != i__base)						\
		   TEGRA_WRITE32(&i__base[i__reg], i__data);		\
	   else {							\
		   LTRACEF("Device base address undefined -- write ignored\n"); \
	   }								\
	   LOG_LWRNG_WRITE32(#reg);					\
	})
#endif

#ifndef LWRNG_GET_REG
#define LWRNG_GET_REG(baseaddr, reg)					\
	({ uint32_t i__reg = (reg);					\
	   uint32_t i__data = 0U;					\
	   uint8_t *i__base = (baseaddr);				\
	   if (NULL != i__base)						\
		   i__data = TEGRA_READ32(&i__base[i__reg]);		\
	   else {							\
		   LTRACEF("Device base address undefined -- read ignored\n"); \
	   }								\
	   LOG_LWRNG_READ32(#reg);					\
	   i__data;							\
	})
#endif
#else

#ifndef LWRNG_SET_REG
#define LWRNG_SET_REG(baseaddr, reg, data) TEGRA_WRITE32(&(baseaddr)[reg], data)
#endif

#ifndef LWRNG_GET_REG
#define LWRNG_GET_REG(baseaddr, reg)	   TEGRA_READ32(&(baseaddr)[reg])
#endif

#endif /* TRACE_REG_OPS */

#ifndef LWRNG_GET_REG_NOTRACE
#define LWRNG_GET_REG_NOTRACE(baseaddr, reg) TEGRA_READ32(&(baseaddr)[reg])
#endif

static uint32_t lwrng_status_is_good;

#if 0
static status_t lwrng_device_disabled_check(uint8_t *lwrng_base)
{
	status_t ret = NO_ERROR;

	(void)lwrng_base;

	LTRACEF("entry\n");

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif // 0

#if 0
/* KAT test failure check => TODO
 */
static bool is_kat_failure(uint32_t alarms)
{
	(void)alarms;
	return false;
}

/* Check alarms => TODO
 */
static status_t lwrng_check_errors(uint8_t *lwrng_base,
				  uint32_t istat,
				  uint32_t *alarms_p,
				  bool *kat_failure_p)
{
	status_t ret = NO_ERROR;

	(void)lwrng_base;
	(void)istat;
	(void)alarms_p;
	(void)kat_failure_p;

	LTRACEF("entry\n");

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif // 0

#if 0
static status_t lwrng_check_state(uint8_t *lwrng_base,
				     bool wait_for_rdy,
				     bool *kat_failure_p)
{
	status_t ret = NO_ERROR;

	(void)lwrng_base;
	(void)wait_for_rdy;
	(void)kat_failure_p;

	LTRACEF("entry\n");
	LTRACEF("exit: %d\n", ret);

	return ret;
}
#endif

#if LWRNG_DISABLE_ON_KAT_ERROR
/* This totally disables the LWRNG unit until next system reset.
 *
 * It is called if the LWRNG fails to initialize properly after LWRNG reset;
 * the initialization is retried LWRNG_INIT_RETRY_COUNT times.
 */
static void lwrng_disable(uint8_t *lwrng_base)
{
	(void)lwrng_base;

	LTRACEF("entry\n");
	LTRACEF("exit\n");
}
#endif /* LWRNG_DISABLE_ON_KAT_ERROR */

#if LWRNG_INIT_SOFT_RESET
static void lwrng_status_unknown(void)
{
	lwrng_status_is_good = 0U;
}

/* Soft reset LWRNG.
 */
static status_t lwrng_reset(uint8_t *lwrng_base)
{
	status_t ret    = NO_ERROR;
	uint32_t regval = 0U;
	uint32_t rmask  = 0U;
	uint32_t count  = 0U;

	LTRACEF("entry\n");

	lwrng_status_unknown();

#if USE_DEV_LWRNG_HEADER
	regval = LW_DRF_DEF(LW_LWRNG, R_CTRL1, SOFT_RST, TRUE);
#else
	/* This field begins at bit 0 */
	regval = HW_SET_BITFIELD(LW_LWRNG_R_CTRL1_SOFT_RST_TRUE, 0x1U, 0U);
#endif
	rmask = regval;
	LWRNG_SET_REG(lwrng_base, LWRNG_REGNAME(LW_LWRNG_R_CTRL1), regval);

	/* HW clears SOFT_RST bit once reset complete
	 */
	do {
		if (count > LWRNG_TIMEOUT) {
			ret = SE_ERROR(ERR_TIMED_OUT);
			break;
		}
		count++;

		regval = LWRNG_GET_REG_NOTRACE(lwrng_base, LWRNG_REGNAME(LW_LWRNG_R_CTRL1));
	} while ((regval & rmask) != 0U);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* LWRNG_INIT_SOFT_RESET */

#if defined(POLL_LWRNG_RSTAT_STARTUP)
/* This would block for < 2000 cycles if LWRNG setup health tests take that long.
 * (assuming STARTUP_DONE not set before this).
 *
 * But neither R_ISTAT register nor R_STAT are implemented in CMOD
 * => can not call this.
 */
static status_t lwrng_wait_for_init_step_done(uint8_t *lwrng_base)
{
	status_t ret = NO_ERROR;
	uint32_t count = 1U;
	uint32_t stat = 0U;
	uint32_t regval = 0U;

#if USE_DEV_LWRNG_HEADER
	const uint32_t istat_done = LW_LWRNG_R_ISTAT_0_STARTUP_DONE_TRUE;
#else
	const uint32_t istat_done = LW_LWRNG_R_ISTAT_STARTUP_DONE_TRUE;
#endif

	LTRACEF("entry\n");

	/* Poll until R_ISTATE field STARTUP_DONE is set or timeout.
	 */
	do {
		if (count > LWRNG_STARTUP_RETRY_COUNT) {
			ret = SE_ERROR(ERR_BAD_STATE);
			regval = LWRNG_GET_REG(lwrng_base, LWRNG_REGNAME(LW_LWRNG_R_STAT));
			LTRACEF("LWRNG startup failed, R_STAT 0x%x\n", regval);
			break;
		}
		count++;

		regval = LWRNG_GET_REG_NOTRACE(lwrng_base,
					       LWRNG_REGNAME(LW_LWRNG_R_ISTAT));

#if USE_DEV_LWRNG_HEADER
		stat = LW_DRF_VAL(LW_LWRNG, R_ISTAT, STARTUP_DONE, regval);
#else
		/* Shift right 1 bits and mask out one bit after that */
		stat = HW_GET_BITFIELD(regval, 0x1U, 1U);
#endif
	} while (stat != istat_done);
	CCC_ERROR_CHECK(ret);

	/* Read R_STAT register for STARTUP_ERROR field value */
	regval = LWRNG_GET_REG_NOTRACE(lwrng_base,
				       LWRNG_REGNAME(LW_LWRNG_R_STAT));
#if USE_DEV_LWRNG_HEADER
	stat = LW_DRF_VAL(LW_LWRNG, R_STAT, STARTUP_ERROR, regval);
	if (stat != LW_LWRNG_R_STAT_0_STARTUP_ERROR_FALSE) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}
#else
	/* Shift right 10 bits and mask out one bit after that */
	stat = HW_GET_BITFIELD(regval, 0x1U, 10U);
	if (stat != LW_LWRNG_R_STAT_STARTUP_ERROR_FALSE) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* defined(POLL_LWRNG_RSTAT_STARTUP) */

static status_t lwrng_check_status_init(uint8_t *lwrng_base)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	(void)lwrng_base;

#if defined(POLL_LWRNG_RSTAT_STARTUP)
	/* This is necessary after LWRNG exits reset
	 * i.e. at startup and soft resets.
	 */
	ret = lwrng_wait_for_init_step_done(lwrng_base);
	CCC_ERROR_CHECK(ret);
	lwrng_status_is_good = 0x5A5A5AU;
fail:
#else
	lwrng_status_is_good = 0x5A5A5AU;
#endif

	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if LWRNG_INIT_WARMUP || LWRNG_INIT_SOFT_RESET
/* Each read produces 16 bit random value, each word contains
 * validity bit.
 *
 * This is "warm-up" code which may skip validity checks.
 *
 * The warm-up read is no longer done by default,
 * set LWRNG_INIT_WARMUP nonzero to do this.
 */
static inline bool lwrng_drbg16_wup(uint8_t *lwrng_base,
				    uint16_t *rv,
				    bool validity_check)
{
	uint32_t val = 0U;
	uint32_t regval = 0U;
	uint32_t retries = LWRNG_DFIFO_RETRY_COUNT;
	bool valid = false;
	CCC_LOOP_VAR;

#if USE_DEV_LWRNG_HEADER
	const uint32_t vmask = LW_DRF_DEF(LW_LWRNG, DFIFO_OUT, VALID, TRUE);
#else
	/* DFIFO_OUT_VALID flag is in bit 31 (msb) */
	const uint32_t vmask = HW_SET_BITFIELD(LW_LWRNG_DFIFO_OUT_VALID_TRUE, 0x1U, 31U);
#endif

	CCC_LOOP_BEGIN {
		val = LWRNG_GET_REG(lwrng_base, LWRNG_REGNAME(LW_LWRNG_DFIFO_OUT));
		regval = val;

		/* Misra obslwrities */
		if (val >= 0x10000U) {
			val &= 0xFFFFU;
		}

		valid = !validity_check || ((regval & vmask) != 0U);

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

static status_t lwrng_get_random_init_bytes(uint8_t *lwrng_base, uint8_t *rbuf,
					    uint32_t nbytes,
					    bool validity_check)
{
	status_t ret = NO_ERROR;
	uint32_t half_count = nbytes / sizeof_u32(uint16_t);
	uint32_t residue = nbytes % sizeof_u32(uint16_t);
	uint32_t inx = 0U;
	uint16_t v16 = 0U;
	uint8_t *p = NULL;

	LTRACEF("entry (%u bytes)\n", nbytes);

	if (0U == lwrng_status_is_good) {
		ret = lwrng_check_status_init(lwrng_base);
		CCC_ERROR_CHECK(ret);
	}

	if ((NULL == rbuf) || (0U == nbytes)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	p = &rbuf[0];

	for (; inx < half_count; inx++) {
		uint32_t v = 0U;
		if (!BOOL_IS_TRUE(lwrng_drbg16_wup(lwrng_base, &v16, validity_check))) {
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
		if (!BOOL_IS_TRUE(lwrng_drbg16_wup(lwrng_base, &v16, validity_check))) {
			CCC_ERROR_WITH_ECODE(ERR_NOT_VALID);
		}
		*p = BYTE_VALUE(v16);
		p++;
	}

	LOG_HEXBUF("DRNG", rbuf, nbytes);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t lwrng_warmup(uint8_t *lwrng_base, uint32_t rounds)
{
	status_t ret = NO_ERROR;
	static uint8_t rbuf[ LWRNG_INITIAL_BUF_BYTES ];
	uint32_t inx = 0U;

	LTRACEF("entry\n");

	/* Generate some fixed number of bits with the LWRNG, ignore valid bit.
	 */
	ret = lwrng_get_random_init_bytes(lwrng_base, rbuf, LWRNG_INITIAL_BUF_BYTES, false);
	CCC_ERROR_CHECK(ret);

	for (inx = 1U; inx <= rounds; inx++) {
		/* Generate some fixed number of bits with the LWRNG,
		 * do not ignore valid bit.  Retry on validity error.
		 */
		ret = lwrng_get_random_init_bytes(lwrng_base, rbuf,
						  LWRNG_INITIAL_BUF_BYTES, true);
		CCC_ERROR_CHECK(ret);
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* LWRNG_INIT_WARMUP || LWRNG_INIT_SOFT_RESET */

/* Pass the (mapped) LWRNG base address in lwrng_base_address; does not
 * use engine calls.
 */
status_t lwrng_init(uint8_t *lwrng_base_address)
{
	status_t ret = NO_ERROR;
	uint8_t *lwrng_base = NULL;
	uint32_t regval = 0U;

#if MEASURE_ENGINE_PERF(MEASURE_PERF_LWRNG_INIT)
	uint32_t cmd_perf_usec = GET_USEC_TIMESTAMP;
#endif

	LTRACEF("entry: LWRNG base_address %p\n", lwrng_base_address);

	if (NULL == lwrng_base_address) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("LWRNG base address NULL\n"));
	}

	lwrng_base = lwrng_base_address;

#if LOCAL_TRACE
	regval = LWRNG_GET_REG(lwrng_base, LWRNG_REGNAME(LW_LWRNG_HWCFG00));
#if USE_DEV_LWRNG_HEADER
	LTRACEF("[ LWRNG version <major %u, minor %u> ]\n",
		LW_DRF_VAL(LW_LWRNG, HWCFG00, VERSION_MAJOR, regval),
		LW_DRF_VAL(LW_LWRNG, HWCFG00, VERSION_MINOR, regval));
#else
	LTRACEF("[ LWRNG version <major %u, minor %u> ]\n",
		HW_GET_BITFIELD(regval, 0xFU, 0U),
		HW_GET_BITFIELD(regval, 0xFU, 4U));
#endif
#endif /* LOCAL_TRACE */

	/* Initialize PLL for LWRNG and switch clock select for LWRNG
	 * to the on-chip PLL which usually can run hundreds of MHz.
	 */

	/* 2) start LWRNG
	 */
	regval = 0U;

	/* This sequence from SE IAS 6.1.1.1
	 */

	/* SLCG_OVR field is set FALSE by default */
#if USE_DEV_LWRNG_HEADER
	regval = LW_FLD_SET_DRF_DEF(LW_LWRNG, R_CTRL0, SW_ENGINE_ENABLED, TRUE, regval);
#else
	regval = HW_SET_BITFIELD(LW_LWRNG_R_CTRL0_SW_ENGINE_ENABLED_TRUE, 0x1U, 2U);
#endif

	LWRNG_SET_REG(lwrng_base, LWRNG_REGNAME(LW_LWRNG_R_CTRL0), regval);

#if LWRNG_INIT_WARMUP == 0
	ret = lwrng_check_status_init(lwrng_base);
	CCC_ERROR_CHECK(ret);
#else
	ret = lwrng_warmup(lwrng_base, LWRNG_INITIAL_BUF_BYTE_CYCLES);
	CCC_ERROR_CHECK(ret);
#endif /* LWRNG_INIT_WARMUP */

#if LWRNG_INIT_SOFT_RESET
	/* Test that LWRNG works after reset
	 */
	ret = lwrng_reset(lwrng_base);
	CCC_ERROR_CHECK(ret);

	/* Generate one chunk of valid DRNG with LWRNG after reset.
	 */
	ret = lwrng_warmup(lwrng_base, 1U);
	CCC_ERROR_CHECK(ret);
#endif

fail:
#if MEASURE_ENGINE_PERF(MEASURE_PERF_LWRNG_INIT)
	show_lwrng_init_perf("LWRNG init", get_elapsed_usec(cmd_perf_usec));
	CCC_ERROR_MESSAGE("LWRNG init loop count: %u\n", count);
#endif

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_LWRNG_INIT */
