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

#ifndef INCLUDED_CRYPTO_KCONFIG_H
#define INCLUDED_CRYPTO_KCONFIG_H

#include <crypto_ops.h>

#if HAVE_CRYPTO_KCONFIG

/**
 * @brief To enable SE0 AES/PKA0 dynamic keyslots, subsystem needs to
 * set those with kconfig_set_dynamic_keyslot().
 *
 * There is no default keyslots enabled for safety reasons with this,
 * they need to be set by the subsystem, e.g. with this during
 * CCC initialization:
 *
 * status_t crypto_kconfig_init(uint32_t aes_dyn_kslot,
 *                              uint32_t pka0_dyn_kslot);
 *
 *
 * PKA1 does not need dynamic keyslot, CCC uses PKA1 bank registers
 * instead.
 *
 * To use fixed keyslots instead of this runtime setup, leave
 * HAVE_CRYPTO_KCONFIG 0 and CCC will use these instead for
 * dynamic keyslots:
 *
 * SE_AES_PREDEFINED_DYNAMIC_KEYSLOT for the fixed AES keyslot (0..15)
 * SE_RSA_PREDEFINED_DYNAMIC_KEYSLOT for PKA0 fixed RSA keyslot (0..3)
 *
 * If no dynamic keyslot mechanism is available when an operation
 * attempts to use a dynamic key, the operation will fail with
 * ERR_NOT_SUPPORTED error code.
 */

enum kconf_keyset_e {
	KCONF_KEYSET_NONE = 0,
	KCONF_KEYSET_SE_AES,
	KCONF_KEYSET_SE_PKA0,
};

/* ANSI C++ forbids forward decl of enums so se_cdev_id_e from
 * tegra_cdev.h needs to exist here.
 *
 * These functions allow runtime set/get the dynamic keyslot
 * in case it is defined e.g. in a device tree.
 */
status_t kconfig_set_dynamic_keyslot(enum se_cdev_id_e device,
				     enum kconf_keyset_e keyset,
				     uint32_t keyslot,
				     bool enable);

status_t kconfig_get_dynamic_keyslot(enum se_cdev_id_e device,
				     enum kconf_keyset_e keyset,
				     uint32_t *keyslot_p);

status_t crypto_kconfig_init(uint32_t aes_dyn_kslot,
			     uint32_t pka0_dyn_kslot);

#endif /* HAVE_CRYPTO_KCONFIG */
#endif /* INCLUDED_CRYPTO_KCONFIG_H */
