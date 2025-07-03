/*
 * Copyright (c) 2017-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/**
 * @file   crypto_ks_se.c
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2019-05-08 13:12:58
 *
 * @brief  set/clear SE keyslots.
 * Functions used by CCC library mode clients.
 */
#include <crypto_system_config.h>
#include <crypto_ops.h>

#if HAVE_CRYPTO_KCONFIG
#include <crypto_kconfig.h>
#endif /* HAVE_CRYPTO_KCONFIG */

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if HAVE_SE_AES

#if HAVE_CCC_LIBRARY_MODE_CALLS
static status_t aes_keyslot_clear(const engine_t *engine, uint32_t keyslot)
{
	status_t ret = NO_ERROR;
	LTRACEF("entry\n");

	if ((NULL == engine) || (keyslot >= SE_AES_MAX_KEYSLOTS)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (engine->e_id) {
	case CCC_ENGINE_SE0_AES0:
	case CCC_ENGINE_SE0_AES1:
	case CCC_ENGINE_SE1_AES0:
	case CCC_ENGINE_SE1_AES1:
		break;
	default:
		CCC_ERROR_MESSAGE("Invalid AES engine\n");
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	HW_MUTEX_TAKE_ENGINE(engine);
	ret = tegra_se_clear_aes_keyslot_locked(engine, keyslot, true);
	HW_MUTEX_RELEASE_ENGINE(engine);

	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to clear AES keyslot (engine %s)\n",
				      engine->e_name));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Provided for library mode external callers; not used by CCC core.
 */
status_t se_clear_device_aes_keyslot(se_cdev_id_t device_id, uint32_t keyslot)
{
	status_t ret = NO_ERROR;
	const engine_t *engine = NULL;

	LTRACEF("entry\n");

	ret = ccc_select_device_engine(device_id, &engine, CCC_ENGINE_CLASS_AES);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Could not find active device %u with an AES keyslot)\n",
				      device_id));

	ret = aes_keyslot_clear(engine, keyslot);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_keyslot_set_locked(const engine_t *engine, uint32_t keyslot,
				       const uint8_t *keydata, uint32_t keysize_bits)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == engine) ||
	    (NULL == keydata) || !IS_VALID_AES_KEYSIZE_BITS(keysize_bits) ||
	    (keyslot >= SE_AES_MAX_KEYSLOTS)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (engine->e_id) {
	case CCC_ENGINE_SE0_AES0:
	case CCC_ENGINE_SE0_AES1:
	case CCC_ENGINE_SE1_AES0:
	case CCC_ENGINE_SE1_AES1:
		break;
	default:
		CCC_ERROR_MESSAGE("Invalid AES engine\n");
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	ret = aes_write_key_iv_locked(engine,
				      keyslot, keysize_bits,
				      SE_AES_KSLOT_SEL_KEY, keydata);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to set SE keyslot for (engine %s)\n",
					  engine->e_name));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Does not support setting AES-XTS subkeys (two keyslots or a SUBKEY
 * depending on HW). If XTS support required add a new function for
 * setting a subkey, easier than modifying this one (this does not
 * know the algorithm).
 */
status_t se_set_device_aes_keyslot(se_cdev_id_t device_id, uint32_t keyslot,
				   const uint8_t *keydata, uint32_t keysize_bits)
{
	status_t ret = NO_ERROR;
	const engine_t *engine = NULL;

	LTRACEF("entry\n");

	ret = ccc_select_device_engine(device_id, &engine, CCC_ENGINE_CLASS_AES);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Could not find active device %u with an AES keyslot)\n",
					  device_id));

	HW_MUTEX_TAKE_ENGINE(engine);
	ret = aes_keyslot_set_locked(engine, keyslot, keydata, keysize_bits);
	HW_MUTEX_RELEASE_ENGINE(engine);

	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_set_device_aes_keyslot_and_iv(se_cdev_id_t device_id, uint32_t keyslot,
					  const uint8_t *keydata, uint32_t keysize_bits,
					  const uint8_t *oiv)
{
	status_t ret = NO_ERROR;
	const engine_t *engine = NULL;
	bool clear_keyslot = false;
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	ret = ccc_select_device_engine(device_id, &engine, CCC_ENGINE_CLASS_AES);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Could not find active device %u with an AES keyslot)\n",
					  device_id));

	HW_MUTEX_TAKE_ENGINE(engine);

	CCC_LOOP_BEGIN {
		ret = aes_keyslot_set_locked(engine, keyslot, keydata, keysize_bits);
		CCC_ERROR_END_LOOP(ret);

		clear_keyslot = true;

		ret = aes_write_key_iv_locked(engine,
					      keyslot, 128U,
					      SE_AES_KSLOT_SEL_ORIGINAL_IV,
					      oiv);
		/* Value check after loop */
		CCC_LOOP_STOP;
	} CCC_LOOP_END;

	HW_MUTEX_RELEASE_ENGINE(engine);
	CCC_ERROR_CHECK(ret);

	clear_keyslot = false;
fail:
	if (BOOL_IS_TRUE(clear_keyslot)) {
		status_t fret = se_clear_device_aes_keyslot(device_id, keyslot);
		if (NO_ERROR != fret) {
			LTRACEF("AES keyslot clear failed: %d\n", fret);
			/* FALLTHROUGH */
		}
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_set_device_aes_keyslot_iv(se_cdev_id_t device_id, uint32_t keyslot,
				      const uint8_t *oiv)
{
	status_t ret = NO_ERROR;
	const engine_t *engine = NULL;

	LTRACEF("entry\n");

	ret = ccc_select_device_engine(device_id, &engine, CCC_ENGINE_CLASS_AES);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Could not find active device %u with an AES keyslot)\n",
					  device_id));

	HW_MUTEX_TAKE_ENGINE(engine);
	ret = aes_write_key_iv_locked(engine,
				      keyslot, 128U,
				      SE_AES_KSLOT_SEL_ORIGINAL_IV,
				      oiv);
	HW_MUTEX_RELEASE_ENGINE(engine);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_CCC_LIBRARY_MODE_CALLS */

#if HAVE_CRYPTO_KCONFIG || defined(SE_AES_PREDEFINED_DYNAMIC_KEYSLOT)
/* Get a dynamic AES keyslot with this function.
 *
 * TODO: Make sure the dynamic keyslot never has LOCK bit set
 * in families that support it.
 */
status_t aes_engine_get_dynamic_keyslot(const struct se_engine_aes_context *econtext,
					uint32_t *dynamic_keyslot_p)
{
	status_t ret = NO_ERROR;
	uint32_t dkslot = SE_AES_MAX_KEYSLOTS;

	LTRACEF("entry\n");

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

#if HAVE_CRYPTO_KCONFIG
	/* Use a system wide dynamic AES keyslot set
	 * up at runtime (if not => error).
	 *
	 * This can be used if AES keyslot is in device
	 * tree or some other runtime setup location and
	 * a subsystem enables runtime PKA0 keyslot.
	 */
	ret = kconfig_get_dynamic_keyslot(econtext->engine->e_dev->cd_id,
					  KCONF_KEYSET_SE_AES, &dkslot);
	CCC_ERROR_CHECK(ret,
			LTRACEF("No dynamic AES keyslot enabled\n"));
#else
	/* Compile time fixed AES dynamic keyslot is used
	 */
	dkslot = SE_AES_PREDEFINED_DYNAMIC_KEYSLOT;
#endif /* HAVE_CRYPTO_KCONFIG */

	LTRACEF("AES config dynamic keyslot: %u\n", dkslot);

	if (NULL != dynamic_keyslot_p) {
		*dynamic_keyslot_p = dkslot;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_CRYPTO_KCONFIG || defined(SE_AES_PREDEFINED_DYNAMIC_KEYSLOT) */
#endif /* HAVE_SE_AES */
