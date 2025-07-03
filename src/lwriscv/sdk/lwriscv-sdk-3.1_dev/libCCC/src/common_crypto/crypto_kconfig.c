/*
 * Copyright (c) 2018-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/* CCC "dynamic keyslot" configuration runtime setting.
 *
 * CCC "dynamic keyslots" can be managed at runtime by kernel clients.
 * - enable/disable dynamic keyslots
 * - SE0 AES and PKA0 dyn keyslots can be set
 * - SE1 AES and PKA1 dyn keyslots can be set
 *
 * If HAVE_CRYPTO_KCONFIG is zero this code is not used.
 * In that case two pre-defined fixed keyslots can be used:
 *
 * SE_AES_PREDEFINED_DYNAMIC_KEYSLOT for the fixed AES keyslot (0..15)
 * SE_RSA_PREDEFINED_DYNAMIC_KEYSLOT for PKA0 fixed RSA keyslot (0..3)
 *
 * PKA1 does not needs such, it uses bank registers instead for
 * "dynamic keyslot" effect.
 */
#include <crypto_system_config.h>

#if HAVE_CRYPTO_KCONFIG

#include <tegra_cdev.h>
#include <crypto_kconfig.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

/* CCC dynamic keyslot configuration.
 * This is not a keyslot manager. This just allows runtime
 * setting of one AES and one PKA0 keyslot used at runtime
 * when the caller does not specify which keyslot to use.
 *
 * The dynamic keyslot can be disabled/enabled at runtime,
 * but the caller should disable it only after all operations
 * using the dynamic keyslot have completed.
 *
 * Otherwise operations may fail.
 *
 * If the RSA dynamic keyslot is disabled, all new
 * RSA operations will be automatically forwarded to PKA1
 * is it is enabled, even if the caller requests use of
 * SE PKA0.
 *
 * For AES, the operations will fail if a dynamic keyslot
 * is required but not available (e.g. disabled).
 */
struct dynkslot_s {
	uint32_t dyn_keyslot;
	bool     dyn_keyslot_enabled;
};

/* Static global kconfig object
 * This is not really protected for thread access but the intention
 * is it is not changed that often. (If is => protect with SW mutex).
 */
static struct {
	/* The dynamic AES keyslot for CCC */
	struct dynkslot_s se0_aes;

	/* The dynamic SE PKA0 RSA keyslot for CCC */
	struct dynkslot_s se0_pka0;

#if HAVE_SE1
	/* No default dynamic keyslots are set for SE1 engine.
	 * Please explicitly set SE1 operation dynamic keyslots
	 * with the provided routines if required.
	 */

	/* The dynamic AES keyslot for CCC */
	struct dynkslot_s se1_aes;

	/* The dynamic SE PKA0 RSA keyslot for CCC */
	struct dynkslot_s se1_pka0;
#endif /* HAVE_SE1 */

} context_kconfig_options;

status_t kconfig_get_dynamic_keyslot(enum se_cdev_id_e device,
				    enum kconf_keyset_e keyset,
				    uint32_t *keyslot_p)
{
	status_t ret = NO_ERROR;
	/* Default to invalid keyslot (too large for both RSA and AES keyslots) */
	uint32_t ks = SE_AES_MAX_KEYSLOTS;
	const struct dynkslot_s *dks = NULL;

	LTRACEF("entry\n");

	if (NULL == keyslot_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}
	*keyslot_p = SE_AES_MAX_KEYSLOTS;

	switch (device) {
	case SE_CDEV_SE0:
		if (KCONF_KEYSET_SE_AES == keyset) {
			dks = &context_kconfig_options.se0_aes;
		} else if (KCONF_KEYSET_SE_PKA0 == keyset) {
			dks = &context_kconfig_options.se0_pka0;
		} else {
			LTRACEF("SE0 keyset 0x%x does not have dynamic keys\n", keyset);
			ret = SE_ERROR(ERR_ILWALID_ARGS);
		}
		break;

#if HAVE_SE1
	case SE_CDEV_SE1:
		if (KCONF_KEYSET_SE_AES == keyset) {
			dks = &context_kconfig_options.se1_aes;
		} else if (KCONF_KEYSET_SE_PKA0 == keyset) {
			dks = &context_kconfig_options.se1_pka0;
		} else {
			LTRACEF("SE1 keyset 0x%x does not have dynamic keys\n", keyset);
			ret = SE_ERROR(ERR_ILWALID_ARGS);
		}
		break;
#endif /* HAVE_SE1 */

	default:
		LTRACEF("Unsupported device %u\n", device);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (NULL != dks) {
		if (!BOOL_IS_TRUE(dks->dyn_keyslot_enabled)) {
			CCC_ERROR_WITH_ECODE(ERR_NOT_VALID,
					     LTRACEF("SE0 dynamic keyslot not enabled\n"));
		}
		ks = dks->dyn_keyslot;
	}

	*keyslot_p = ks;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/*
 * For SE0 && SE1 AES:
 * If keyslot >= SE_AES_MAX_KEYSLOTS the AES config keyslot is
 * not modified. To disable this dynamic keyslot: set enable
 * false, to enable it set it true.
 *
 * For SE0 && SE1 PKA0:
 * If keyslot >= SE_RSA_MAX_KEYSLOTS the PKA0 config keyslot is
 * not modified. To disable this dynamic keyslot: set enable
 * false, to enable it set it true.
 *
 * For PKA1:
 * This engine does not use dynamic keyslots, it uses bank registers
 * instead.
 */
status_t kconfig_set_dynamic_keyslot(enum se_cdev_id_e device,
				     enum kconf_keyset_e keyset,
				     uint32_t keyslot,
				     bool enable)
{
	status_t ret = NO_ERROR;
	struct dynkslot_s *dks = NULL;

	LTRACEF("entry\n");

	switch (device) {
	case SE_CDEV_SE0:
		if (KCONF_KEYSET_SE_AES == keyset) {
			dks = &context_kconfig_options.se0_aes;
		} else if (KCONF_KEYSET_SE_PKA0 == keyset) {
			dks = &context_kconfig_options.se0_pka0;
		} else {
			LTRACEF("SE0 set keyset 0x%x does not have dynamic keys\n", keyset);
			ret = SE_ERROR(ERR_ILWALID_ARGS);
		}
		break;

#if HAVE_SE1
	case SE_CDEV_SE1:
		if (KCONF_KEYSET_SE_AES == keyset) {
			dks = &context_kconfig_options.se1_aes;
		} else if (KCONF_KEYSET_SE_PKA0 == keyset) {
			dks = &context_kconfig_options.se1_pka0;
		} else {
			LTRACEF("SE1 set keyset 0x%x does not have dynamic keys\n", keyset);
			ret = SE_ERROR(ERR_ILWALID_ARGS);
		}
		break;
#endif /* HAVE_SE1 */

	default:
		LTRACEF("Unsupported device %u\n", device);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (NULL != dks) {
		dks->dyn_keyslot_enabled = enable;

		if (keyslot < SE_AES_MAX_KEYSLOTS) {
			LTRACEF("SE dynamic keyslot set to %u\n", keyslot);
			dks->dyn_keyslot = keyslot;
		}

		LTRACEF("SE set dynamic keyslot %u %s\n",
			dks->dyn_keyslot,
			(enable ? "enabled" : "disabled"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t crypto_kconfig_init(uint32_t aes_dyn_kslot,
			     uint32_t pka0_dyn_kslot)
{
	status_t ret = NO_ERROR;

	if (aes_dyn_kslot >= SE_AES_MAX_KEYSLOTS) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (pka0_dyn_kslot >= SE_RSA_MAX_KEYSLOTS) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = kconfig_set_dynamic_keyslot(SE_CDEV_SE0,
					  KCONF_KEYSET_SE_AES,
					  aes_dyn_kslot,
					  true);
	CCC_ERROR_CHECK(ret);

	ret = kconfig_set_dynamic_keyslot(SE_CDEV_SE0,
					  KCONF_KEYSET_SE_PKA0,
					  pka0_dyn_kslot,
					  true);
	CCC_ERROR_CHECK(ret);
fail:
	return ret;
}
#endif /* HAVE_CRYPTO_KCONFIG */
