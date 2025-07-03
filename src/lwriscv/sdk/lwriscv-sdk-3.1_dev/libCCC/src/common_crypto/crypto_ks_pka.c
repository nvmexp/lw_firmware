/*
 * Copyright (c) 2016-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */
/**
 * @file   crypto_ks_pka.c
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2019-05-08 10:37:46
 *
 * @brief  RSA keyslot functions
 */

#include <crypto_system_config.h>

#if CCC_WITH_RSA

#include <crypto_ops.h>

#if CCC_SOC_WITH_PKA1

/* for key load/clear */
#include <tegra_pka1_rsa.h>

#endif /* CCC_SOC_WITH_PKA1 */

#if CCC_SOC_WITH_LWPKA

#include <lwpka/tegra_lwpka_rsa.h>

#endif /* CCC_SOC_WITH_LWPKA */

#if HAVE_CRYPTO_KCONFIG
#include <crypto_kconfig.h>
#endif /* HAVE_CRYPTO_KCONFIG */

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#include <ccc_safe_arith.h>

/* Support function to clear RSA keyslot.
 */
static status_t rsa_keyslot_clear(const engine_t *engine, uint32_t keyslot)
{
	status_t ret = NO_ERROR;
	LTRACEF("entry\n");

	if ((NULL == engine) || (keyslot >= SE_RSA_MAX_KEYSLOTS)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (engine->e_id) {
	case CCC_ENGINE_SE0_PKA0:
	case CCC_ENGINE_SE1_PKA0:
		break;
	case CCC_ENGINE_PKA:
#if CCC_WITH_PKA_KEYLOAD
		break;
#else
		LTRACEF("PKA RSA keyslots not supported\n");
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
#endif
	default:
		LTRACEF("Invalid RSA engine\n");
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	HW_MUTEX_TAKE_ENGINE(engine);

	switch (engine->e_id) {
#if HAVE_SE_RSA
	case CCC_ENGINE_SE0_PKA0:
	case CCC_ENGINE_SE1_PKA0:
		/* SE engine max RSA byte size is 2048 / 8 = 256 */
		ret = tegra_se_clear_rsa_keyslot_locked(engine, keyslot, 256);
		break;
#endif
#if CCC_WITH_PKA_KEYLOAD
	case CCC_ENGINE_PKA:
		ret = PKA_CLEAR_RSA_KEYSLOT_LOCKED(engine,
						   keyslot,
						   MAX_RSA_MODULUS_BYTE_SIZE);
		break;
#endif
	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}

	HW_MUTEX_RELEASE_ENGINE(engine);

	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to clear RSA keyslot (engine %s)\n",
				engine->e_name));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Clear the selected RSA keyslot in an active DEVICE (specified by device_id).
 */
status_t se_clear_device_rsa_keyslot(se_cdev_id_t device_id, uint32_t keyslot)
{
	status_t ret = NO_ERROR;
	const engine_t *engine = NULL;

	LTRACEF("entry\n");

	ret = ccc_select_device_engine(device_id, &engine, CCC_ENGINE_CLASS_RSA);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Could not find active device %u with an RSA keyslot)\n",
					  device_id));

	ret = rsa_keyslot_clear(engine, keyslot);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_keyslot_set_check_args(const engine_t *engine,
					   uint32_t keyslot,
					   uint32_t exponent_size_bytes,
					   uint32_t rsa_size_bytes,
					   const uint8_t *exponent,
					   const uint8_t *modulus)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* PKA1 RSA keyslot max == SE RSA keyslot max */
	if ((NULL == engine) ||
	    (NULL == modulus) || (NULL == exponent) ||
	    !IS_VALID_RSA_KEYSIZE_BITS(rsa_size_bytes << 3U) ||
	    !((exponent_size_bytes == 4U) ||
	      IS_VALID_RSA_KEYSIZE_BITS(exponent_size_bytes << 3U)) ||
	    (keyslot >= SE_RSA_MAX_KEYSLOTS)) {
		ret = SE_ERROR(ERR_ILWALID_ARGS);
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_check_key(uint32_t exponent_size_bytes,
			      uint32_t rsa_size_bytes,
			      const uint8_t *exponent,
			      const uint8_t *modulus,
			      bool is_big_endian,
			      bool *is_valid_p)
{
	status_t ret = NO_ERROR;
	bool is_valid = false;

	LTRACEF("entry\n");

	if (NULL == is_valid_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	is_valid = *is_valid_p;
	*is_valid_p = false;

	/* High order bit of modulus must be set as well */
	is_valid = (se_util_vec_is_msb_set(modulus, rsa_size_bytes, is_big_endian) &&
		    is_valid);

	/* modulus and exponent must be odd */
	is_valid = (se_util_vec_is_odd(modulus, rsa_size_bytes, is_big_endian) &&
		    is_valid);
	is_valid = (se_util_vec_is_odd(exponent, exponent_size_bytes, is_big_endian) &&
		    is_valid);

	*is_valid_p = is_valid;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_check_engine_keyslots_supported(engine_id_t e_id)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch (e_id) {
#if HAVE_SE_RSA
	case CCC_ENGINE_SE0_PKA0:
	case CCC_ENGINE_SE1_PKA0:
		break;
#endif /* HAVE_SE_RSA */
	case CCC_ENGINE_PKA:
#if CCC_WITH_PKA_KEYLOAD
		break;
#else
		LTRACEF("PKA RSA keyslots not supported\n");
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
#endif
	default:
		LTRACEF("Invalid RSA engine\n");
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_rsa_pka_engine_write_key(const engine_t *engine,
					    uint32_t keyslot,
					    uint32_t exponent_size_bytes,
					    uint32_t rsa_size_bytes,
					    const uint8_t *exponent,
					    const uint8_t *modulus,
					    bool is_big_endian,
					    const rsa_montgomery_values_t *mg,
					    const char **load_id_p)
{
	status_t ret = NO_ERROR;

	(void)load_id_p;

	LTRACEF("entry\n");

	(void)keyslot;
	(void)exponent_size_bytes;
	(void)rsa_size_bytes;
	(void)exponent;
	(void)modulus;
	(void)is_big_endian;
	(void)mg;

	switch (engine->e_id) {
#if HAVE_SE_RSA
	case CCC_ENGINE_SE0_PKA0:
	case CCC_ENGINE_SE1_PKA0:
		if (NULL != mg) {
			LTRACEF("Montgomery values ignored for SE RSA engine\n");
		}
		ret = rsa_write_key_locked(engine, keyslot,
					   exponent_size_bytes, rsa_size_bytes,
					   exponent, modulus,
					   is_big_endian, is_big_endian);
#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
		*load_id_p = "SE PKA0 key load";
#endif
		break;
#endif /* HAVE_SE_RSA */

#if CCC_WITH_PKA_KEYLOAD
	case CCC_ENGINE_PKA:
		ret = PKA_RSA_WRITE_KEY_LOCKED(engine, keyslot,
					       exponent_size_bytes, rsa_size_bytes,
					       exponent, modulus,
					       is_big_endian, is_big_endian, mg);
#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
		*load_id_p = "PKA key load";
#endif
		break;
#endif
	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_keyslot_set(const engine_t *engine, uint32_t keyslot,
				uint32_t exponent_size_bytes, uint32_t rsa_size_bytes,
				const uint8_t *exponent, const uint8_t *modulus,
				bool is_big_endian, const rsa_montgomery_values_t *mg)
{
	status_t ret = NO_ERROR;
	bool is_valid = true;
	const char *load_id = "keyslot load";

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
	uint32_t perf_usec = GET_USEC_TIMESTAMP;
#endif

	LTRACEF("entry\n");

	ret = rsa_keyslot_set_check_args(engine, keyslot, exponent_size_bytes,
					 rsa_size_bytes, exponent, modulus);
	CCC_ERROR_CHECK(ret);

	ret = rsa_check_key(exponent_size_bytes, rsa_size_bytes,
			    exponent, modulus, is_big_endian, &is_valid);
	if ((!BOOL_IS_TRUE(is_valid)) || (NO_ERROR != ret)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("RSA modulus or exponent invalid\n"));
	}

	ret = rsa_check_engine_keyslots_supported(engine->e_id);
	CCC_ERROR_CHECK(ret);

	LTRACEF("Set dev rsa key: expsize=%u rsasize=%u keyslot=%u\n",
		exponent_size_bytes, rsa_size_bytes, keyslot);

	HW_MUTEX_TAKE_ENGINE(engine);

	ret = se_rsa_pka_engine_write_key(engine, keyslot,
					  exponent_size_bytes, rsa_size_bytes,
					  exponent, modulus,
					  is_big_endian, mg,
					  &load_id);

	HW_MUTEX_RELEASE_ENGINE(engine);

	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to set RSA keyslot (engine %s) [%s]\n",
				engine->e_name, load_id));

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
	show_elapsed_time(load_id, perf_usec);
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if CCC_WITH_PKA_KEYLOAD
#if CCC_SOC_WITH_PKA1
static status_t rsa_keyslot_montgomery_check_args(uint32_t rsa_size_bytes,
						  const uint8_t *r2,
						  const uint8_t *m_prime)
{
	status_t ret = NO_ERROR;
	uint32_t rsa_size_bits = 0U;

	LTRACEF("entry\n");

	if ((NULL == m_prime) || (NULL == r2)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("RSA montgomery out of band values not set\n"));
	}

	CCC_SAFE_MULT_U32(rsa_size_bits, rsa_size_bytes, 8U);
	if (!IS_VALID_RSA_KEYSIZE_BITS(rsa_size_bits)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Unsupported RSA size\n"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_keyslot_montgomery_r2_check(uint32_t rsa_size_bytes,
						const uint8_t *modulus,
						const uint8_t *r2,
						bool is_big_endian)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;
	uint32_t eflags = 0U;

	LTRACEF("entry\n");

	if (!BOOL_IS_TRUE(is_big_endian)) {
		/* All relevant parameter values must be in the same endian */
		eflags = CMP_FLAGS_VEC1_LE | CMP_FLAGS_VEC2_LE;
	}

	if (BOOL_IS_TRUE(se_util_vec_is_zero(r2, rsa_size_bytes, &rbad))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Invalid R^2 Montgomery value\n"));
	}

	if (NO_ERROR != rbad) {
		CCC_ERROR_WITH_ECODE(rbad);
	}

	rbad = ERR_BAD_STATE;

	if (!BOOL_IS_TRUE(se_util_vec_cmp_endian(modulus,
						 r2,
						 rsa_size_bytes,
						 eflags, &rbad))) {
		CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID,
				     LTRACEF("Too large R^2 Montgomery value\n"));
	}

	if (NO_ERROR != rbad) {
		CCC_ERROR_WITH_ECODE(rbad);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t rsa_keyslot_montgomery_m_prime_check(uint32_t rsa_size_bytes,
						     const uint8_t *m_prime)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(se_util_vec_is_zero(m_prime, rsa_size_bytes, &rbad))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Invalid M' Montgomery value\n"));
	}

	if (NO_ERROR != rbad) {
		CCC_ERROR_WITH_ECODE(rbad);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_SOC_WITH_PKA1 */

/* Reduces to rsa_keyslot_set() when montgomery values are not used.
 *
 * With PKA1 the provided montgomery value arguments are verified here
 * and ignored if system does not need them. If not provided they get
 * callwlated down the call chain (relatively slow operation).
 *
 * This is available for compatibility even in platforms that do not
 * need to be provided montgomery values when loading keyslots is
 * supported.
 */
status_t se_set_device_rsa_keyslot_montgomery(
	se_cdev_id_t device_id, uint32_t keyslot,
	uint32_t exponent_size_bytes, uint32_t rsa_size_bytes,
	const uint8_t *exponent, const uint8_t *modulus,
	bool is_big_endian, const uint8_t *m_prime,
	const uint8_t *r2)
{
	status_t ret = NO_ERROR;
	const engine_t *engine = NULL;
	rsa_montgomery_values_t *mgp = NULL;

	LTRACEF("entry\n");

	/* In case engine does not need montgomery values they
	 * are just ignored.
	 */
	(void)m_prime;
	(void)r2;

#if CCC_SOC_WITH_PKA1
	rsa_montgomery_values_t mg;

	if ((NULL != m_prime) || (NULL != r2)) {
		ret = rsa_keyslot_montgomery_check_args(rsa_size_bytes, r2,
							m_prime);
		CCC_ERROR_CHECK(ret);

		ret = rsa_keyslot_montgomery_r2_check(rsa_size_bytes,
						      modulus, r2,
						      is_big_endian);
		CCC_ERROR_CHECK(ret);

		ret = rsa_keyslot_montgomery_m_prime_check(rsa_size_bytes,
							   m_prime);
		CCC_ERROR_CHECK(ret);

		/* M_PRIME is not "mod MODULUS" so the upper bound can
		 * not be verified.
		 *
		 * I think it is "mod R" but value R is not available.
		 */
		mg.m_prime    = &m_prime[0];
		mg.r2         = &r2[0];
		mgp           = &mg;
	}
#endif /* CCC_SOC_WITH_PKA1 */

	ret = ccc_select_device_engine(device_id, &engine, CCC_ENGINE_CLASS_RSA);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Could not find active device %u with an RSA keyslot)\n",
				device_id));

	ret = rsa_keyslot_set(engine, keyslot,
			      exponent_size_bytes, rsa_size_bytes,
			      exponent, modulus, is_big_endian, mgp);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_PKA_KEYLOAD */

/* Set RSA keyslot in specified device (SE0, SE1 or PKA1)
 *
 * The modulus and exponent are both in the same byte order (BE if is_big_endian is true).
 *
 * Exponent must be 4 bytes long (pubkeys) or same as RSA SIZE (privatekeys)
 * RSA size must be one of the supported RSA sizes in bytes (e.g. 2048/8 = 256 bytes).
 *
 * Caller should keep MODULUS and EXPONENT buffers word aligned!
 */
status_t se_set_device_rsa_keyslot(se_cdev_id_t device_id, uint32_t keyslot,
				   uint32_t exponent_size_bytes, uint32_t rsa_size_bytes,
				   const uint8_t *exponent, const uint8_t *modulus,
				   bool is_big_endian)
{
	status_t ret = NO_ERROR;
	LTRACEF("entry\n");

#if HAVE_PKA1_RSA
	if (SE_CDEV_PKA1 == device_id) {
		/* Loading montgomery function exists in PKA1 */
		ret = se_set_device_rsa_keyslot_montgomery(device_id, keyslot, exponent_size_bytes,
							   rsa_size_bytes, exponent, modulus,
							   is_big_endian, NULL, NULL);
		CCC_ERROR_CHECK(ret);
	}
#endif

#if CCC_SOC_WITH_LWPKA
#if HAVE_LWPKA_RSA
	if (SE_CDEV_PKA1 == device_id) {
		const engine_t *engine = NULL;

		ret = ccc_select_device_engine(device_id, &engine, CCC_ENGINE_CLASS_RSA);
		CCC_ERROR_CHECK(ret,
				LTRACEF("Could not find active device %u with an RSA keyslot)\n",
					device_id));

		ret = rsa_keyslot_set(engine, keyslot,
				      exponent_size_bytes, rsa_size_bytes,
				      exponent, modulus, is_big_endian, NULL);
		CCC_ERROR_CHECK(ret);
	}
#endif
#endif /* CCC_SOC_WITH_LWPKA */

#if HAVE_SE_RSA
	if (SE_CDEV_SE0 == device_id) {
		const engine_t *engine = NULL;

		ret = ccc_select_device_engine(device_id, &engine, CCC_ENGINE_CLASS_RSA);
		CCC_ERROR_CHECK(ret,
				LTRACEF("Could not find active device %u with an RSA keyslot)\n",
					device_id));

		ret = rsa_keyslot_set(engine, keyslot,
				      exponent_size_bytes, rsa_size_bytes,
				      exponent, modulus, is_big_endian, NULL);
		CCC_ERROR_CHECK(ret);
	}
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_CRYPTO_KCONFIG || defined(SE_RSA_PREDEFINED_DYNAMIC_KEYSLOT)
/* Find a dynamic RSA keyslot for an RSA operation for PKA0 engine.
 *
 * For PKA1 engine the dynamic keyslot is not defined; the caller is
 * expected to always use the PKA1 bank registers instead when a
 * dynamic keyslot is needed (returns an invalid keyslot value with
 * NO_ERROR in that case).
 *
 * For PKA0 a dynamic keyslot may be needed and the context keyslot is
 * returned if defined. If not defined, try to find a configured
 * keyslot instead.
 *
 * RSA engine context field rsa_dynamic_keyslot is set only by the
 * subsystem using CCC.
 *
 * If the flag RSA_FLAGS_DYNAMIC_KEYSLOT is set, that keyslot can be
 * freely used by CCC for all RSA operations related to this context.
 */
status_t pka0_engine_get_dynamic_keyslot(const struct se_engine_rsa_context *econtext,
					 uint32_t *dynamic_keyslot_p)
{
	status_t ret = NO_ERROR;
	uint32_t dkslot = SE_RSA_MAX_KEYSLOTS;

	LTRACEF("entry\n");

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (econtext->engine->e_id) {
	case CCC_ENGINE_PKA:
		/* PKA uses bank registers in this case */
		LTRACEF("PKA engines uses register memory, not a dynamic keyslot\n");
		break;

	case CCC_ENGINE_SE0_PKA0:
	case CCC_ENGINE_SE1_PKA0:
		{
#if HAVE_CRYPTO_KCONFIG
			/* Use a system wide dynamic RSA keyslot set
			 * up at runtime (if not => error).
			 *
			 * This can be used if PKA0 keyslot is in device
			 * tree or some other runtime setup location and
			 * a subsystem enables runtime PKA0 keyslot.
			 */
			ret = kconfig_get_dynamic_keyslot(econtext->engine->e_dev->cd_id,
							  KCONF_KEYSET_SE_PKA0, &dkslot);
			if (NO_ERROR != ret) {
				LTRACEF("No dynamic PKA0 keyslot enabled\n");
				break;
			}
			LTRACEF("PKA0 config dynamic keyslot: %u\n", dkslot);
#else
			/* Compile time fixed PKA0 dynamic keyslot is used
			 */
			dkslot = SE_RSA_PREDEFINED_DYNAMIC_KEYSLOT;
			LTRACEF("PKA0 config dynamic keyslot: %u\n", dkslot);
#endif /* HAVE_CRYPTO_KCONFIG */
		}
		break;

	default:
		LTRACEF("Engine %u does not use RSA keys\n",
			econtext->engine->e_id);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (NULL != dynamic_keyslot_p) {
		*dynamic_keyslot_p = dkslot;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_CRYPTO_KCONFIG || defined(SE_RSA_PREDEFINED_DYNAMIC_KEYSLOT) */
#endif /* CCC_WITH_RSA */
