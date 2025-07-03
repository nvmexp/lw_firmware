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

/**
 * @file   tegra_cdev.c
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2015
 *
 * @brief CheetAh SE/PKA1/RNG1 engine switch implementation.
 *
 * This file contains the variables and functions for engine switch.
 */

/* Static device and engine configuration and lookup functions.
 */
#include <crypto_system_config.h>
#include <crypto_ops.h>

#if CCC_SOC_WITH_LWPKA

#if HAVE_LWPKA_RSA
#include <lwpka/tegra_lwpka_rsa.h>
#endif
#if HAVE_LWPKA_ECC
#include <lwpka/tegra_lwpka_ec.h>
#endif
#if HAVE_LWPKA_MODULAR
#include <lwpka/tegra_lwpka_mod.h>
#endif
#if HAVE_LWRNG_DRNG
#include <tegra_lwrng.h>
#endif

#endif /* CCC_SOC_WITH_LWPKA */

#if CCC_SOC_WITH_PKA1

#if HAVE_PKA1_RSA
#include <tegra_pka1_rsa.h>
#endif
#if HAVE_PKA1_ECC
#include <tegra_pka1_ec.h>
#endif
#if HAVE_PKA1_MODULAR
#include <tegra_pka1_mod.h>
#endif
#if HAVE_RNG1_DRNG
#include <tegra_rng1.h>
#endif
#if HAVE_PKA1_TRNG
#include <tegra_pka1_gen.h>
#endif

#endif /* CCC_SOC_WITH_PKA1 */

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#define SYSTEM_INITIALIZED 0xeac3feedU

/* These get polled and the base address set and state updated in
 * tegra_init_crypto_devices() at runtime.
 */
static se_cdev_t cdev_SE0 = {
	.cd_name  = "SE0",
	.cd_id    = SE_CDEV_SE0,
	.cd_state = SE_CDEV_STATE_OFF,
	.cd_base  = CCC_BASE_WHEN_DEVICE_NOT_PRESENT,
};

static se_cdev_t cdev_PKA = {
	.cd_name  = "PKA",
	.cd_id    = SE_CDEV_PKA1,
	.cd_state = SE_CDEV_STATE_OFF,
	.cd_base  = CCC_BASE_WHEN_DEVICE_NOT_PRESENT,
};

static se_cdev_t cdev_RNG = {
#if HAVE_LWRNG_DRNG
	.cd_name  = "LWRNG",
#else
	.cd_name  = "RNG1",
#endif /* HAVE_LWRNG_DRNG */
	.cd_id    = SE_CDEV_RNG1,
	.cd_state = SE_CDEV_STATE_OFF,
	.cd_base  = CCC_BASE_WHEN_DEVICE_NOT_PRESENT,
};

#if HAVE_SE1
static se_cdev_t cdev_SE1 = {
	.cd_name  = "SE1 (second SE engine in T21X)",
	.cd_id    = SE_CDEV_SE1,
	.cd_state = SE_CDEV_STATE_OFF,
	.cd_base  = CCC_BASE_WHEN_DEVICE_NOT_PRESENT,
};
#endif /* HAVE_SE1 */

/**
 * @brief WHIRLPOOL, SW SHA-3, MD5 and any other possible "software
 * engine" use this object as a device in struct engine_s e_dev field
 * (not supported by HW).
 *
 * @defgroup cdev_software Software pseudo-engine
 */
#if HAVE_MD5 || HAVE_SW_WHIRLPOOL || HAVE_SW_SHA3 || CCC_WITH_XMSS
/*@{*/
static se_cdev_t software = {
	.cd_name = "Software",
	.cd_state = SE_CDEV_STATE_NONEXISTENT,
	.cd_base = CCC_BASE_WHEN_DEVICE_NOT_PRESENT,
};
/*@}*/
#endif

#if HAVE_SOC_FEATURE_T23X_MMIO_RANGE

/* SE unit specific register in-use ranges:
 *
 * AES0 => 0x1000U - 0x1fffU
 * AES1 => 0x2000U - 0x2fffU
 * RSA  => PKA0 no longer exists.
 * SHA  => 0x4000U - 0x4fffU
 */
#if HAVE_SE1
/* Secondary SE unit like this only in T21X family
 */
#error "Unit does not support SE1 device"
#endif

#if HAVE_SE_RSA
#error "Unit does not support PKA0"
#endif

#define SE_AES0_ENGINE_REGOFF_START	0x1000U
#define SE_AES0_ENGINE_REGOFF_END	0x1fffU

#define SE_AES1_ENGINE_REGOFF_START	0x2000U
#define SE_AES1_ENGINE_REGOFF_END	0x2fffU

#define SE_SHA_ENGINE_REGOFF_START	0x4000U
#define SE_SHA_ENGINE_REGOFF_END	0x4fffU

#else

/* SE0 (and T21X SE1) unit specific register in-use ranges:
 *
 * SHA	=> 0x100U - 0x18lw
 * AES0 => 0x200U - 0x2f8U
 * AES1 => 0x400U - 0x4f8U
 * RSA  => 0x600U - 0x760U
 */

/* Register offsets for the SE engines in units
 * up to T19X
 */
#define SE_AES0_ENGINE_REGOFF_START	0x200U
#define SE_AES0_ENGINE_REGOFF_END	0x2ffU

#define SE_AES1_ENGINE_REGOFF_START	0x400U
#define SE_AES1_ENGINE_REGOFF_END	0x4ffU

#define SE_PKA0_ENGINE_REGOFF_START	0x600U
#define SE_PKA0_ENGINE_REGOFF_END	0x7ffU

#define SE_SHA_ENGINE_REGOFF_START	0x100U
#define SE_SHA_ENGINE_REGOFF_END	0x1ffU

#endif /* HAVE_SOC_FEATURE_T23X_MMIO_RANGE */

#if CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T23X)
/* The two AES engines are identical in T23X
 *
 * The CLASS DRNG needs special handling; if GENRND is not supported
 * by AES class the engine lookup will then find LWRNG instead
 * (if enabled).
 *
 * IAS recommends using GENRND instead of LWRNG directly and AES1 is
 * before LWRNG in engine table, so that recommendation is dealt with
 * when looking up a generic DRNG engine. If AES1 does not support
 * DRNG then LWRNG is found and used.
 */
#if HAVE_SE_GENRND
#define CCC_AES0_ENGINE_CLASSES (CCC_ENGINE_CLASS_AES | CCC_ENGINE_CLASS_CMAC_AES | \
				 CCC_ENGINE_CLASS_AES_XTS | CCC_ENGINE_CLASS_KW | CCC_ENGINE_CLASS_DRNG)
#else
#define CCC_AES0_ENGINE_CLASSES (CCC_ENGINE_CLASS_AES | CCC_ENGINE_CLASS_CMAC_AES | \
				 CCC_ENGINE_CLASS_AES_XTS | CCC_ENGINE_CLASS_KW)
#endif

#define CCC_AES1_ENGINE_CLASSES CCC_AES0_ENGINE_CLASSES

#else

/* T19X and T18X AES engine supported operation classes differ.
 */
#if HAVE_SE_RANDOM
#define CCC_AES0_ENGINE_CLASSES (CCC_ENGINE_CLASS_AES | CCC_ENGINE_CLASS_CMAC_AES | CCC_ENGINE_CLASS_DRNG)
#else
#define CCC_AES0_ENGINE_CLASSES (CCC_ENGINE_CLASS_AES | CCC_ENGINE_CLASS_CMAC_AES)
#endif

/* AES-XTS not in T18x, but it gets an error otherwise.
 */
#define CCC_AES1_ENGINE_CLASSES (CCC_ENGINE_CLASS_AES | CCC_ENGINE_CLASS_CMAC_AES | \
				 CCC_ENGINE_CLASS_AES_XTS | CCC_ENGINE_CLASS_KW)
#endif /* CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T23X) */

/**
 * @brief Engine mapping table.
 *
 * Engines are looked up with capability vector defined in e_class field
 * with functions ccc_select_engine() and ccc_select_device_engine()
 *
 * Note on the ccc_engine size in a PSC ROM compilation, which only
 *  enables a subset of the algorithms:
 *   On PSC (64 bit Risc-V) when using function pointers
 *   the size of this table is 1848 bytes.
 *
 * When the function pointers are replaced by enums the size is
 *   reduced to 1064 bytes.
 *
 * When the function pointers are replaced by uint8_t the size is
 *   reduced to 504 bytes.
 *
 * @ingroup eng_switch_api
 * @defgroup eng_switch_table Engine lookup table
 */
/*@{*/
static const engine_t ccc_engine[] = {

#if HAVE_RNG1_DRNG
	/* Elliptic RNG1 engine */
	{
		.e_name = "RNG1 engine",
		.e_dev  = &cdev_RNG,
		.e_id   = CCC_ENGINE_RNG1,
		.e_class= CCC_ENGINE_CLASS_DRNG,
		.e_do_remap = CFALSE,
		.e_proc = {
			.p_rng = FUNTAG(ft_rng, engine_rng1_drng_generate_locked),
		},
		.e_mutex = {
			.m_lock   = FUNTAG(ft_m_lock, rng1_acquire_mutex),
			.m_unlock = FUNTAG(ft_m_unlock, rng1_release_mutex),
		},
	},
#elif HAVE_LWRNG_DRNG
	/* LWRNG engine */
	{
		.e_name = "LWRNG engine",
		.e_dev  = &cdev_RNG,
		.e_id   = CCC_ENGINE_RNG1,
		.e_class= CCC_ENGINE_CLASS_DRNG,
		.e_do_remap = CFALSE,
		.e_proc = {
			.p_rng = FUNTAG(ft_rng, engine_lwrng_drng_generate_locked),
		},
		.e_mutex = {
			.m_lock   = FUNTAG(ft_m_lock, lwrng_acquire_mutex),
			.m_unlock = FUNTAG(ft_m_unlock, lwrng_release_mutex),
		},
	},
#endif	/* HAVE_LWRNG_DRNG */

#if HAVE_SE_AES
	/* SE0 device engines */
#if HAVE_SE_AES_ENGINE_AES1
	{
		.e_name = "AES1 engine",
		.e_dev  = &cdev_SE0,
		.e_id   = CCC_ENGINE_SE0_AES1,

		.e_class= CCC_AES1_ENGINE_CLASSES,

		.e_regs_begin = SE_AES1_ENGINE_REGOFF_START,
		.e_regs_end   = SE_AES1_ENGINE_REGOFF_END,
		.e_do_remap = CTRUE,
		.e_remap_range = {
			/* AES0 register range accessed via AES1 engine
			 * will get mapped to AES1 register range.
			 *
			 * Other registers are not mapped when
			 * accessing this engine.
			 */
			.rm_start = SE_AES0_ENGINE_REGOFF_START,
			.rm_end   = SE_AES0_ENGINE_REGOFF_END,
		},
		.e_proc = {
			.p_aes  = FUNTAG(ft_aes, engine_aes_process_block_locked),
#if HAVE_CMAC_AES
			.p_cmac = FUNTAG(ft_aes, engine_cmac_process_block_locked),
#endif
#if HAVE_SE_ASYNC_AES
			/* Any client using Async ops is responsible of calling finish after
			 * starting the operation.
			 */
			.p_aes_async_done    = FUNTAG(ft_aes_poll, engine_aes_locked_async_done),
			.p_aes_async_start   = FUNTAG(ft_aes_async, engine_aes_locked_async_start),
			.p_aes_async_finish  = FUNTAG(ft_aes_async, engine_aes_locked_async_finish),
#endif
#if HAVE_SE_KAC_KEYOP
			.p_aes_kwuw  = FUNTAG(ft_aes, engine_aes_keyop_process_locked),
#endif
#if HAVE_SE_GENRND
			.p_rng = FUNTAG(ft_rng, engine_genrnd_generate_locked),
#endif
		},
		.e_mutex = {
			.m_lock   = FUNTAG(ft_m_lock, tegra_se0_mutex_lock),
			.m_unlock = FUNTAG(ft_m_unlock, tegra_se0_mutex_release),
		},
	},
#endif /* HAVE_SE_AES_ENGINE_AES1 */

#if HAVE_SE_AES_ENGINE_AES0
	{
		.e_name = "AES0 engine [AES/RNG]",
		.e_dev  = &cdev_SE0,
		.e_id   = CCC_ENGINE_SE0_AES0,
		.e_class = CCC_AES0_ENGINE_CLASSES,
		.e_regs_begin = SE_AES0_ENGINE_REGOFF_START,
		.e_regs_end   = SE_AES0_ENGINE_REGOFF_END,
		.e_do_remap   = CFALSE,
		.e_proc = {
			.p_aes  = FUNTAG(ft_aes, engine_aes_process_block_locked),
#if HAVE_CMAC_AES
			.p_cmac = FUNTAG(ft_aes, engine_cmac_process_block_locked),
#endif
#if HAVE_SE_GENRND
			.p_rng = FUNTAG(ft_rng, engine_genrnd_generate_locked),
#elif HAVE_SE_RANDOM
			.p_rng = FUNTAG(ft_rng, engine_rng0_generate_locked),
#endif
#if HAVE_SE_ASYNC_AES
			/* Any client using Async ops is responsible of calling finish after
			 * starting the operation.
			 */
			.p_aes_async_done    = FUNTAG(ft_aes_poll, engine_aes_locked_async_done),
			.p_aes_async_start   = FUNTAG(ft_aes_async, engine_aes_locked_async_start),
			.p_aes_async_finish  = FUNTAG(ft_aes_async, engine_aes_locked_async_finish),
#endif
#if HAVE_SE_KAC_KEYOP
			.p_aes_kwuw  = FUNTAG(ft_aes, engine_aes_keyop_process_locked),
#endif
		},
		.e_mutex = {
			.m_lock   = FUNTAG(ft_m_lock, tegra_se0_mutex_lock),
			.m_unlock = FUNTAG(ft_m_unlock, tegra_se0_mutex_release),
		},
	},
#endif /* HAVE_SE_AES_ENGINE_AES0 */
#endif /* HAVE_SE_AES */

#if HAVE_SE_RSA
	{
		.e_name = "PKA0 RSA engine",
		.e_dev  = &cdev_SE0,
		.e_id   = CCC_ENGINE_SE0_PKA0,
		.e_class= CCC_ENGINE_CLASS_RSA,
		.e_regs_begin = SE_PKA0_ENGINE_REGOFF_START,
		.e_regs_end   = SE_PKA0_ENGINE_REGOFF_END,
		.e_do_remap = CFALSE,
		.e_proc = {
			.p_rsa_exp  = FUNTAG(ft_rsa, engine_rsa_modular_exp_locked),
		},
		.e_mutex = {
			.m_lock   = FUNTAG(ft_m_lock, tegra_se0_mutex_lock),
			.m_unlock = FUNTAG(ft_m_unlock, tegra_se0_mutex_release),
		},
	},
#endif /* HAVE_SE_RSA */

#if HAVE_SE_SHA
	{
		.e_name = "SHA engine",
		.e_dev  = &cdev_SE0,
		.e_id   = CCC_ENGINE_SE0_SHA,
#if HAVE_HW_SHA3
		.e_class= CCC_ENGINE_CLASS_SHA | CCC_ENGINE_CLASS_SHA3,
#else
		.e_class= CCC_ENGINE_CLASS_SHA,
#endif
		.e_regs_begin = SE_SHA_ENGINE_REGOFF_START,
		.e_regs_end   = SE_SHA_ENGINE_REGOFF_END,
		.e_do_remap = CFALSE,
		.e_proc = {
			.p_sha  = FUNTAG(ft_sha, engine_sha_process_block_locked),
#if HAVE_SE_KAC_KDF || HAVE_HW_HMAC_SHA
			/* Keyed SHA engine operation */
			.p_sha_keyop  = FUNTAG(ft_sha, engine_sha_keyop_process_locked),
#endif

#if HAVE_SE_ASYNC_SHA
			/* Any client using Async ops is responsible of calling finish after
			 * starting the operation.
			 */
			.p_sha_async_done    = FUNTAG(ft_sha_poll, engine_sha_locked_async_done),
			.p_sha_async_start   = FUNTAG(ft_sha_async, engine_sha_locked_async_start),
			.p_sha_async_finish  = FUNTAG(ft_sha_async, engine_sha_locked_async_finish),
#endif
		},
		.e_mutex = {
			.m_lock   = FUNTAG(ft_m_lock, tegra_se0_mutex_lock),
			.m_unlock = FUNTAG(ft_m_unlock, tegra_se0_mutex_release),
		},
	},
#endif /* HAVE_SE_SHA */

#if CCC_SOC_WITH_LWPKA

#if HAVE_LWPKA_RSA
	/* LWPKA device engine for RSA modular exponentiations.
	 *
	 * The LWPKA for RSA is the LW engine also handling ECC
	 * they are separated here logically for colwenience.
	 *
	 * Because of this, the MUTEX functions for these "logical engines" must
	 * depend on the same mutex!
	 *
	 * The basic modular operations can be handled by either engine.
	 * But lwrrently these ops are used only by ECDSA...
	 */
	{
		.e_name = "LWPKA engine [RSA mode]",
		.e_dev  = &cdev_PKA,
		.e_id   = CCC_ENGINE_PKA,
		.e_class= CCC_ENGINE_CLASS_RSA | CCC_ENGINE_CLASS_RSA_4096,
		.e_do_remap = CFALSE,
		.e_proc = {
			.p_rsa_exp  = FUNTAG(ft_rsa, engine_lwpka_rsa_modular_exp_locked),
		},
		.e_mutex = {
			.m_lock   = FUNTAG(ft_m_lock, lwpka_acquire_mutex),
			.m_unlock = FUNTAG(ft_m_unlock, lwpka_release_mutex),
		},
	},
#endif /* HAVE_LWPKA_RSA */

#if HAVE_LWPKA_ECC
	{
		.e_name = "LWPKA engine [ECC mode]",
		.e_dev  = &cdev_PKA,
		.e_id   = CCC_ENGINE_PKA,
		.e_class= CCC_ENGINE_CLASS_EC | CCC_ENGINE_CLASS_P521,
		.e_do_remap = CFALSE,
		.e_proc = {
			.p_ec_p_mult		= FUNTAG(ft_ec, engine_lwpka_ec_point_mult_locked),
			.p_ec_p_verify		= FUNTAG(ft_ec, engine_lwpka_ec_point_verify_locked),
			.p_ec_p_shamir_trick	= FUNTAG(ft_ec, engine_lwpka_ec_shamir_trick_locked),
#if HAVE_LWPKA11_SUPPORT
			.p_ec_ks_ecdsa_sign	= FUNTAG(ft_ec, engine_lwpka_ec_ks_ecdsa_sign_locked),
#if HAVE_LWPKA11_ECKS_KEYOPS
			.p_ec_ks_keyop		= FUNTAG(ft_ec, engine_lwpka_ec_ks_keyop_locked),
#endif
#endif /* HAVE_LWPKA11_SUPPORT */

		// HAVE_LWPKA_MODULAR

			.p_mod_multiplication = FUNTAG(ft_mod, engine_lwpka_modular_multiplication_locked),
			.p_mod_addition       = FUNTAG(ft_mod, engine_lwpka_modular_addition_locked),
			.p_mod_subtraction    = FUNTAG(ft_mod, engine_lwpka_modular_subtraction_locked),
			.p_mod_reduction      = FUNTAG(ft_mod, engine_lwpka_modular_reduction_locked),
#if CCC_WITH_MODULAR_DIVISION
			.p_mod_division       = FUNTAG(ft_mod, engine_lwpka_modular_division_locked),
#endif /* CCC_WITH_MODULAR_DIVISION */
			.p_mod_ilwersion      = FUNTAG(ft_mod, engine_lwpka_modular_ilwersion_locked),
			.p_mod_bit_reduction_dp = FUNTAG(ft_mod, engine_lwpka_modular_dp_reduction_locked),

#if HAVE_GEN_MULTIPLY
			/* Not really a modular operation! This is generic double precision
			 * multiplication.
			 */
			.p_gen_multiplication = FUNTAG(ft_mod, engine_lwpka_gen_mult_locked),

			/* Combines GEN_MULTIPLICATION and REDUCTION
			 *  in one operation.
			 *
			 * This results in a single precision (x*y) mod z value.
			 */
			.p_mod_gen_multiplication = FUNTAG(ft_mod, engine_lwpka_gen_mult_mod_locked),
#endif /* HAVE_GEN_MULTIPLY */
		},
		.e_mutex = {
			.m_lock   = FUNTAG(ft_m_lock, lwpka_acquire_mutex),
			.m_unlock = FUNTAG(ft_m_unlock, lwpka_release_mutex),
		},
	},

#if HAVE_LWPKA_X25519
	/* Add another "virtual engine" for handling the Lwrve25519 operations
	 * This is implemented with LWPKA device => use LWPKA mutex locks and e_dev.
	 */
	{
		.e_name = "LWPKA engine [X25519 mode]",
		.e_dev  = &cdev_PKA,
		.e_id   = CCC_ENGINE_PKA,
		.e_class= CCC_ENGINE_CLASS_X25519,
		.e_do_remap = CFALSE,
		.e_proc = {
			/* C25519/C448 point multiplication */
			.p_ec_p_mult = FUNTAG(ft_ec, engine_lwpka_montgomery_point_mult_locked),
		},
		.e_mutex = {
			.m_lock   = FUNTAG(ft_m_lock, lwpka_acquire_mutex),
			.m_unlock = FUNTAG(ft_m_unlock, lwpka_release_mutex),
		},
	},
#endif /* HAVE_LWPKA_X25519 */

#if HAVE_LWPKA_ED25519 || HAVE_LWPKA_ED448
	{
		.e_name = "LWPKA engine [EDWARDS mode]",
		.e_dev  = &cdev_PKA,
		.e_id   = CCC_ENGINE_PKA,
		.e_class= CCC_ENGINE_CLASS_ED25519,
		.e_do_remap = CFALSE,
		.e_proc = {
			.p_ec_p_mult		= FUNTAG(ft_ec, engine_lwpka_edwards_point_mult_locked),
			.p_ec_p_verify		= FUNTAG(ft_ec, engine_lwpka_edwards_point_verify_locked),
			.p_ec_p_shamir_trick	= FUNTAG(ft_ec, engine_lwpka_edwards_shamir_trick_locked),

			/* Modular operations */
			.p_mod_multiplication = FUNTAG(ft_mod, engine_lwpka_modular_multiplication_locked),
			.p_mod_addition       = FUNTAG(ft_mod, engine_lwpka_modular_addition_locked),
			.p_mod_subtraction    = FUNTAG(ft_mod, engine_lwpka_modular_subtraction_locked),
			.p_mod_reduction      = FUNTAG(ft_mod, engine_lwpka_modular_reduction_locked),
#if CCC_WITH_MODULAR_DIVISION
			.p_mod_division       = FUNTAG(ft_mod, engine_lwpka_modular_division_locked),
#endif /* CCC_WITH_MODULAR_DIVISION */
			.p_mod_ilwersion      = FUNTAG(ft_mod, engine_lwpka_modular_ilwersion_locked),
			.p_mod_bit_reduction_dp = FUNTAG(ft_mod, engine_lwpka_modular_dp_reduction_locked),

			.p_mod_c25519_exp  = FUNTAG(ft_mod, engine_lwpka_modular_exponentiation_locked),
			.p_mod_c25519_sqr  = FUNTAG(ft_mod, engine_lwpka_modular_square_locked),
			.p_mod_c25519_mult = FUNTAG(ft_mod, engine_lwpka_modular_multiplication_locked),

#if HAVE_GEN_MULTIPLY
			/* Combined DP GEN_MULTIPLICATION and BIT SERIAL DP REDUCTION
			 *  without extracting the DP intermediate result.
			 *
			 * This results in a single precision (x*y) mod z value.
			 */
			.p_mod_gen_multiplication = FUNTAG(ft_mod, engine_lwpka_gen_mult_mod_locked),
#endif /* HAVE_GEN_MULTIPLY */
		},
		.e_mutex = {
			.m_lock   = FUNTAG(ft_m_lock, lwpka_acquire_mutex),
			.m_unlock = FUNTAG(ft_m_unlock, lwpka_release_mutex),
		},
	},
#endif /* HAVE_LWPKA_ED25519 */

#endif /* HAVE_LWPKA_ECC */

#else /* PKA1 device, not LWPKA */

#if HAVE_PKA1_RSA
	/* PKA1 device engines.
	 *
	 * The PKA1 for RSA is the same engine
	 * which does ECC, they are separated here logically
	 * for colwenience.
	 *
	 * Because of this, the MUTEX functions for these "logical engines" must
	 * depend on the same mutex!
	 *
	 * The basic modular operations can be handled by either engine.
	 * But lwrrently these ops are used only by ECDSA...
	 */
	{
		.e_name = "PKA1 engine [RSA mode]",
		.e_dev  = &cdev_PKA,
		.e_id   = CCC_ENGINE_PKA,
		.e_class= CCC_ENGINE_CLASS_RSA | CCC_ENGINE_CLASS_RSA_4096,
		.e_do_remap = CFALSE,
		.e_proc = {
			.p_rsa_exp  = FUNTAG(ft_rsa, engine_pka1_rsa_modular_exp_locked),
		},
		.e_mutex = {
			.m_lock   = FUNTAG(ft_m_lock, pka1_acquire_mutex),
			.m_unlock = FUNTAG(ft_m_unlock, pka1_release_mutex),
		},
	},
#endif /* HAVE_PKA1_RSA */

#if HAVE_PKA1_ECC
	{
		.e_name = "PKA1 engine [ECC mode]",
		.e_dev  = &cdev_PKA,
		.e_id   = CCC_ENGINE_PKA,
		.e_class= CCC_ENGINE_CLASS_EC,
		.e_do_remap = CFALSE,
		.e_proc = {
			.p_ec_p_mult		= FUNTAG(ft_ec, engine_pka1_ec_point_mult_locked),
#if HAVE_PKA1_ECC_POINT_DOUBLE
			.p_ec_p_double		= FUNTAG(ft_ec, engine_pka1_ec_point_double_locked),
#endif /* HAVE_PKA1_ECC_POINT_DOUBLE */
#if HAVE_PKA1_ECC_POINT_ADD
			.p_ec_p_add		= FUNTAG(ft_ec, engine_pka1_ec_point_add_locked),
#endif /* HAVE_PKA1_ECC_POINT_ADD */
			.p_ec_p_verify		= FUNTAG(ft_ec, engine_pka1_ec_point_verify_locked),
#if HAVE_SHAMIR_TRICK
			.p_ec_p_shamir_trick	= FUNTAG(ft_ec, engine_pka1_ec_shamir_trick_locked),
#endif /* HAVE_SHAMIR_TRICK */

#if HAVE_PKA1_MODULAR
			.p_mod_multiplication = FUNTAG(ft_mod, engine_pka1_modular_multiplication_locked),
			.p_mod_addition       = FUNTAG(ft_mod, engine_pka1_modular_addition_locked),
			.p_mod_subtraction    = FUNTAG(ft_mod, engine_pka1_modular_subtraction_locked),
			.p_mod_reduction      = FUNTAG(ft_mod, engine_pka1_modular_reduction_locked),
#if CCC_WITH_MODULAR_DIVISION
			.p_mod_division       = FUNTAG(ft_mod, engine_pka1_modular_division_locked),
#endif /* CCC_WITH_MODULAR_DIVISION */
			.p_mod_ilwersion      = FUNTAG(ft_mod, engine_pka1_modular_ilwersion_locked),
#if CCC_WITH_BIT_SERIAL_REDUCTION
			.p_mod_bit_reduction  = FUNTAG(ft_mod, engine_pka1_bit_serial_reduction_locked),
#endif /* CCC_WITH_BIT_SERIAL_REDUCTION */
			.p_mod_bit_reduction_dp =
				FUNTAG(ft_mod, engine_pka1_bit_serial_double_precision_reduction_locked),
#endif
#if HAVE_GEN_MULTIPLY
			/* Not really a modular operation! This is generic double precision
			 * multiplication.
			 */
			.p_gen_multiplication = FUNTAG(ft_mod, engine_pka1_gen_mult_locked),

			/* Combined DP GEN_MULTIPLICATION and BIT SERIAL DP REDUCTION
			 *  without extracting the DP intermediate result.
			 *
			 * This results in a single precision (x*y) mod z value.
			 */
			.p_mod_gen_multiplication = FUNTAG(ft_mod, engine_pka1_gen_mult_mod_locked),
#endif /* HAVE_GEN_MULTIPLY */
		},
		.e_mutex = {
			.m_lock   = FUNTAG(ft_m_lock, pka1_acquire_mutex),
			.m_unlock = FUNTAG(ft_m_unlock, pka1_release_mutex),
		},
	},

#if HAVE_PKA1_X25519 && HAVE_ELLIPTIC_20
	/* Add another "virtual engine" for handling the Lwrve25519 operations
	 * This is implemented with PKA1 device => use PKA1 mutex locks and e_dev.
	 */
	{
		.e_name = "PKA1 engine [X25519 mode]",
		.e_dev  = &cdev_PKA,
		.e_id   = CCC_ENGINE_PKA,
		.e_class= CCC_ENGINE_CLASS_X25519,
		.e_do_remap = CFALSE,
		.e_proc = {
			/* C25519 modular multiplication */
			.p_ec_p_mult = FUNTAG(ft_ec, engine_pka1_c25519_point_mult_locked),
		},
		.e_mutex = {
			.m_lock   = FUNTAG(ft_m_lock, pka1_acquire_mutex),
			.m_unlock = FUNTAG(ft_m_unlock, pka1_release_mutex),
		},
	},
#endif /* HAVE_PKA1_X25519 && HAVE_ELLIPTIC_20 */

#if HAVE_PKA1_ED25519 && HAVE_ELLIPTIC_20
	{
		.e_name = "PKA1 engine [ED25519 mode]",
		.e_dev  = &cdev_PKA,
		.e_id   = CCC_ENGINE_PKA,
		.e_class= CCC_ENGINE_CLASS_ED25519,
		.e_do_remap = CFALSE,
		.e_proc = {
			.p_ec_p_mult		= FUNTAG(ft_ec, engine_pka1_ed25519_point_mult_locked),
#if HAVE_PKA1_ED25519_POINT_DOUBLE
			.p_ec_p_double		= FUNTAG(ft_ec, engine_pka1_ed25519_point_double_locked),
#endif /* HAVE_PKA1_ED25519_POINT_DOUBLE */
#if HAVE_PKA1_ED25519_POINT_ADD
			.p_ec_p_add		= FUNTAG(ft_ec, engine_pka1_ed25519_point_add_locked),
#endif /* HAVE_PKA1_ED25519_POINT_ADD */
			.p_ec_p_verify		= FUNTAG(ft_ec, engine_pka1_ed25519_point_verify_locked),
#if HAVE_SHAMIR_TRICK
			.p_ec_p_shamir_trick	= FUNTAG(ft_ec, engine_pka1_ed25519_shamir_trick_locked),
#endif /* HAVE_SHAMIR_TRICK */

#if HAVE_PKA1_MODULAR
			/* Dolwmented in Elliptic spec:
			 * DWC_pka_2_11a-ea01_EdDSA_Ed25519_recipe.pdf
			 *
			 * Special purpose C25519 modular operations used
			 * for the Ed25519 lwrve in PKA1
			 *
			 * These use Eulers algorithm which is slower than the
			 * Euclidian algorithm used to callwlate similar modular
			 * values, but the Euler's version is time ilwariant which
			 * is a required property in Bernstein's paper for Lwrve25519 ops =>
			 * use these ones for the modular operations when possible when the
			 * modulo is p=2^255-19
			 */
			.p_mod_c25519_exp  = FUNTAG(ft_mod, engine_pka1_c25519_mod_exp_locked),
			.p_mod_c25519_sqr  = FUNTAG(ft_mod, engine_pka1_c25519_mod_square_locked),
			.p_mod_c25519_mult = FUNTAG(ft_mod, engine_pka1_c25519_mod_mult_locked),

			/* Note: ed25519 modular => should probably disable some of the
			 * below modular operations just to force the previous
			 * comment above to be taken seriously => TODO (later)
			 */
			.p_mod_multiplication = FUNTAG(ft_mod, engine_pka1_modular_multiplication_locked),
			.p_mod_addition       = FUNTAG(ft_mod, engine_pka1_modular_addition_locked),
			.p_mod_subtraction    = FUNTAG(ft_mod, engine_pka1_modular_subtraction_locked),
			.p_mod_reduction      = FUNTAG(ft_mod, engine_pka1_modular_reduction_locked),
#if CCC_WITH_MODULAR_DIVISION
			.p_mod_division       = FUNTAG(ft_mod, engine_pka1_modular_division_locked),
#endif /* CCC_WITH_MODULAR_DIVISION */
			.p_mod_ilwersion      = FUNTAG(ft_mod, engine_pka1_modular_ilwersion_locked),
#if CCC_WITH_BIT_SERIAL_REDUCTION
			.p_mod_bit_reduction  = FUNTAG(ft_mod, engine_pka1_bit_serial_reduction_locked),
#endif /* CCC_WITH_BIT_SERIAL_REDUCTION */
			.p_mod_bit_reduction_dp =
				 FUNTAG(ft_mod, engine_pka1_bit_serial_double_precision_reduction_locked),

#if HAVE_GEN_MULTIPLY
			/* Combined DP GEN_MULTIPLICATION and BIT SERIAL DP REDUCTION
			 *  without extracting the DP intermediate result.
			 *
			 * This results in a single precision (x*y) mod z value.
			 */
			.p_mod_gen_multiplication = FUNTAG(ft_mod, engine_pka1_gen_mult_mod_locked),
#endif /* HAVE_GEN_MULTIPLY */
#endif /* HAVE_PKA1_MODULAR */
		},
		.e_mutex = {
			.m_lock   = FUNTAG(ft_m_lock, pka1_acquire_mutex),
			.m_unlock = FUNTAG(ft_m_unlock, pka1_release_mutex),
		},
	},
#endif /* HAVE_PKA1_ED25519 && HAVE_ELLIPTIC_20 */

#if HAVE_P521 && HAVE_ELLIPTIC_20
	{
		.e_name = "PKA1 engine [P521 mode]",
		.e_dev  = &cdev_PKA,
		.e_id   = CCC_ENGINE_PKA,
		.e_class= CCC_ENGINE_CLASS_P521,
		.e_do_remap = CFALSE,
		.e_proc = {
			.p_ec_p_mult		= FUNTAG(ft_ec, engine_pka1_ec_p521_point_mult_locked),
#if HAVE_PKA1_P521_POINT_DOUBLE
			.p_ec_p_double		= FUNTAG(ft_ec, engine_pka1_ec_p521_point_double_locked),
#endif /* HAVE_PKA1_P521_POINT_DOUBLE */
#if HAVE_PKA1_P521_POINT_ADD
			.p_ec_p_add		= FUNTAG(ft_ec, engine_pka1_ec_p521_point_add_locked),
#endif /* HAVE_PKA1_P521_POINT_ADD */
			.p_ec_p_verify		= FUNTAG(ft_ec, engine_pka1_ec_p521_point_verify_locked),
#if HAVE_SHAMIR_TRICK
			.p_ec_p_shamir_trick	= FUNTAG(ft_ec, engine_pka1_ec_p521_shamir_trick_locked),
#endif /* HAVE_SHAMIR_TRICK */

#if HAVE_PKA1_MODULAR
			/* Generic operands, but modulus must be set to p = 2^251-1
			 * Operation => MODMULT_521
			 */
			.p_mod_mult_m521 = FUNTAG(ft_mod, engine_pka1_modular_m521_multiplication_locked),

			/* Arbitrary modulus; operand size 521 bits (0 <= operand < m)
			 *
			 * If the following modular operations are used => the required montgomery values
			 *  must be callwlated Out-Of-Band!!
			 *
			 * The PKA1 is not capable of callwlating them with the provided montgomery
			 * precompute functions!
			 *
			 * Operation => M_521_MONTMULT
			 */
			.p_mod_mult_mont521 = FUNTAG(ft_mod, engine_pka1_modular_multiplication521_locked),

			/* If required for P521 => these must also get montgomery values OOB!
			 *
			 * (XXXX TODO: verify that these also work for 2^251-1 modulus!
			 *  XXXX  Not explicitly stated in Elliptic doc, only the mult function
			 *  XXXX  is said "not to function with P521" and the P521 mult functions are
			 *  XXXX  provided separately).
			 */
			.p_mod_addition       = FUNTAG(ft_mod, engine_pka1_modular_addition_locked),
			.p_mod_subtraction    = FUNTAG(ft_mod, engine_pka1_modular_subtraction_locked),
			.p_mod_reduction      = FUNTAG(ft_mod, engine_pka1_modular_reduction_locked),
#if CCC_WITH_MODULAR_DIVISION
			.p_mod_division       = FUNTAG(ft_mod, engine_pka1_modular_division_locked),
#endif /* CCC_WITH_MODULAR_DIVISION */
			.p_mod_ilwersion      = FUNTAG(ft_mod, engine_pka1_modular_ilwersion_locked),
#if CCC_WITH_BIT_SERIAL_REDUCTION
			.p_mod_bit_reduction  = FUNTAG(ft_mod, engine_pka1_bit_serial_reduction_locked),
#endif /* CCC_WITH_BIT_SERIAL_REDUCTION */
			.p_mod_bit_reduction_dp =
				FUNTAG(ft_mod, engine_pka1_bit_serial_double_precision_reduction_locked),
#endif
		},
		.e_mutex = {
			.m_lock   = FUNTAG(ft_m_lock, pka1_acquire_mutex),
			.m_unlock = FUNTAG(ft_m_unlock, pka1_release_mutex),
		},
	},
#endif /* HAVE_P521 && HAVE_ELLIPTIC_20 */

#endif /* HAVE_PKA1_ECC */

#if HAVE_PKA1_TRNG
	{
		/* Uses PKA1 mutexes; this is inside PKA1 unit.
		 */
		.e_name = "PKA1 TRNG engine",
		.e_dev  = &cdev_PKA,
		.e_id   = CCC_ENGINE_PKA,
		.e_class= CCC_ENGINE_CLASS_TRNG,
		.e_do_remap = CFALSE,
		.e_proc = {
			.p_rng = FUNTAG(ft_rng, engine_pka1_trng_generate_locked),
		},
		.e_mutex = {
			.m_lock   = FUNTAG(ft_m_lock, pka1_acquire_mutex),
			.m_unlock = FUNTAG(ft_m_unlock, pka1_release_mutex),
		},
	},
#endif /* HAVE_PKA1_TRNG */

#endif /* CCC_SOC_WITH_LWPKA */

#if HAVE_SE1
#if HAVE_SE_AES
	{
		.e_name = "AES1 engine (SE1)",
		.e_dev  = &cdev_SE1,
		.e_id   = CCC_ENGINE_SE1_AES1,

		// TODO: Not sure if SE1 AES1 supports XTS in T21X?
		.e_class= CCC_ENGINE_CLASS_AES | CCC_ENGINE_CLASS_CMAC_AES,
		.e_regs_begin = SE_AES1_ENGINE_REGOFF_START,
		.e_regs_end   = SE_AES1_ENGINE_REGOFF_END,
		.e_do_remap = CTRUE,
		.e_remap_range = {
			/* AES0 register range accessed via AES1 engine
			 * will get mapped to AES1 register range.
			 *
			 * Other registers are not mapped when
			 * accessing this engine.
			 */
			.rm_start = SE_AES0_ENGINE_REGOFF_START,
			.rm_end   = SE_AES0_ENGINE_REGOFF_END,
		}
		.e_proc = {
			.p_aes  = FUNTAG(ft_aes, engine_aes_process_block_locked),
#if HAVE_CMAC_AES
			.p_cmac = FUNTAG(ft_aes, engine_cmac_process_block_locked),
#endif
		},
		// I have no idea how to treat SE1 mutexes or if they even exist => please give info
		.e_mutex = {
			.m_lock   = FUNTAG(ft_m_lock, tegra_se1_mutex_lock),
			.m_unlock = FUNTAG(ft_m_unlock, tegra_se1_mutex_release),
		},
	},

	/* SE1 device engines, only in instances of T21X family */
	{
		.e_name = "AES0 engine [AES/RNG] (SE1)",
		.e_dev  = &cdev_SE1,
		.e_id   = CCC_ENGINE_SE1_AES0,
#if HAVE_SE_RANDOM
		.e_class= CCC_ENGINE_CLASS_AES | CCC_ENGINE_CLASS_CMAC_AES | CCC_ENGINE_CLASS_DRNG,
#else
		.e_class= CCC_ENGINE_CLASS_AES | CCC_ENGINE_CLASS_CMAC_AES,
#endif
		.e_regs_begin = SE_AES0_ENGINE_REGOFF_START,
		.e_regs_end   = SE_AES0_ENGINE_REGOFF_END,
		.e_do_remap = CFALSE,
		.e_proc = {
			.p_aes  = FUNTAG(ft_aes, engine_aes_process_block_locked),
#if HAVE_CMAC_AES
			.p_cmac = FUNTAG(ft_aes, engine_cmac_process_block_locked),
#endif
#if HAVE_SE_RANDOM
			.p_rng = FUNTAG(ft_rng, engine_rng0_generate_locked),
#endif
		},
		// I have no idea how to treat SE1 mutexes or if they even exist => please give info
		.e_mutex = {
			.m_lock   = FUNTAG(ft_m_lock, tegra_se1_mutex_lock),
			.m_unlock = FUNTAG(ft_m_unlock, tegra_se1_mutex_release),
		},
	},
#endif /* HAVE_SE_AES */

#if HAVE_SE_RSA
	{
		.e_name = "PKA0 RSA engine (SE1)",
		.e_dev  = &cdev_SE1,
		.e_id   = CCC_ENGINE_SE1_PKA0,
		.e_class= CCC_ENGINE_CLASS_RSA,
		.e_regs_begin = SE_PKA0_ENGINE_REGOFF_START,
		.e_regs_end   = SE_PKA0_ENGINE_REGOFF_END,
		.e_do_remap = CFALSE,
		.e_proc = {
			.p_rsa_exp  = FUNTAG(ft_rsa, engine_rsa_modular_exp_locked),
		},
	},
#endif

	{
		.e_name = "SHA engine (SE1)",
		.e_dev  = &cdev_SE1,
		.e_id   = CCC_ENGINE_SE1_SHA,
		.e_class= CCC_ENGINE_CLASS_SHA,
		.e_regs_begin = SE_SHA_ENGINE_REGOFF_START,
		.e_regs_end   = SE_SHA_ENGINE_REGOFF_END,
		.e_do_remap = CFALSE,
		.e_proc = {
			.p_sha  = FUNTAG(ft_sha, engine_sha_process_block_locked),
		},
		// I have no idea how to treat SE1 mutexes or if they even exist => please give info
		.e_mutex = {
			.m_lock   = FUNTAG(ft_m_lock, tegra_se1_mutex_lock),
			.m_unlock = FUNTAG(ft_m_unlock, tegra_se1_mutex_release),
		},
	},
#endif /* HAVE_SE1 */

/* CCC supported SW only algorithms.
 */
#if HAVE_MD5
	{
		.e_name = "MD5 (SW version)",
		.e_dev  = &software,
		.e_id   = CCC_ENGINE_SOFT,
		.e_class= CCC_ENGINE_CLASS_MD5,
		.e_do_remap = CFALSE,
	},
#define CCC_USING_SOFT_ENGINE
#endif /* HAVE_MD5 */

#if HAVE_SW_WHIRLPOOL
	{
		.e_name = "WHIRLPOOL (SW version)",
		.e_dev  = &software,
		.e_id   = CCC_ENGINE_SOFT,
		.e_class= CCC_ENGINE_CLASS_WHIRLPOOL,
		.e_do_remap = CFALSE,
	},
#define CCC_USING_SOFT_ENGINE
#endif /* HAVE_SW_WHIRLPOOL */

#if HAVE_SW_SHA3
	{
		.e_name = "SHA3 (SW version)",
		.e_dev  = &software,
		.e_id   = CCC_ENGINE_SOFT,
		.e_class= CCC_ENGINE_CLASS_SHA3,
		.e_do_remap = CFALSE,
	},
#define CCC_USING_SOFT_ENGINE
#endif /* HAVE_SW_SHA3 */

#if CCC_WITH_XMSS
	{
		.e_name = "XMSS (SW version)",
		.e_dev  = &software,
		.e_id   = CCC_ENGINE_SOFT,
		.e_class= CCC_ENGINE_CLASS_XMSS,
		.e_do_remap = CFALSE,
	},
#define CCC_USING_SOFT_ENGINE
#endif /* CCC_WITH_XMSS */

};  /**< Engine lookup table */
/*@}*/

#if !defined(TEGRA_INIT_CRYPTO_PROTO_DECLARED)
/* If this prototype is not declared by tegra_cdev.h
 * do it here, for the function implementation.
 */
status_t tegra_init_crypto_devices(void);
#endif

#if HAVE_NO_STATIC_DATA_INITIALIZER
/* Fix broken non-zero static file global initializers.
 */
static void static_initialize_nonzero(void)
{
#if HAVE_SE_SHA || HAVE_SE_AES || HAVE_SE_RSA || HAVE_SE_GENRND
	ccc_static_init_se_values();
#endif

#if HAVE_LWPKA_RSA || HAVE_LWPKA_ECC || HAVE_LWPKA_MODULAR
	ccc_static_init_lwpka_values();
#endif
}
#endif /* HAVE_NO_STATIC_DATA_INITIALIZER */

/* get base addreses for all devices and mark them active...
 */
status_t tegra_init_crypto_devices(void)
{
	status_t ret = NO_ERROR;
	uint32_t i = 0U;
	const uint32_t elem_size = sizeof_u32(se_cdev_t *);

	static uint32_t tegra_se_hw_initialized;

	/* lookup table for for searching devices to init.
	 * The base address is set as returned by the callee.
	 *
	 * If device is not present, base is set to
	 * CCC_BASE_WHEN_DEVICE_NOT_PRESENT
	 */
	static se_cdev_t *hw_cdev[] = {
		&cdev_SE0,
		&cdev_PKA,
		&cdev_RNG,
#if HAVE_SE1
		&cdev_SE1,
#endif
	};

	const uint32_t asize = sizeof_u32(hw_cdev) / elem_size;

	switch(tegra_se_hw_initialized) {
	case SYSTEM_INITIALIZED:
		CCC_ERROR_MESSAGE("SE driver already initialized\n");
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	case 0U:
		break;
	default:
		CCC_ERROR_MESSAGE("SE driver init value invalid\n");
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}
	CCC_ERROR_CHECK(ret);

	for (i = 0U; i < asize; i++) {
		uint8_t *base = CCC_BASE_WHEN_DEVICE_NOT_PRESENT;

		if (SE_CDEV_STATE_NONEXISTENT == hw_cdev[i]->cd_state) {
			hw_cdev[i]->cd_base = CCC_BASE_WHEN_DEVICE_NOT_PRESENT;
			continue;
		}

		/* Map the base address && system specific device init (if any)
		 *
		 * Subsystem source implementing SYSTEM_MAP_DEVICE() function
		 * needs to include crypto_api.h to know the se_cdev_id_t
		 * (cd_id field type). If it does, you can pass the enum type
		 * without casting it to uint32_t by defining the
		 * CCC_SYSTEM_MAP_DEVICE_ARGUMENT_NO_CASTING macro nonzero.
		 *
		 * If left undefined or has value zero, the first argument will
		 * be cast to uint32_t (backwards compatible). The second argument
		 * is an address of a uint8_t pointer.
		 */
#if CCC_SYSTEM_MAP_DEVICE_ARGUMENT_NO_CASTING
		/* Misra clean way: pass cd_id value with correct type */
		ret = SYSTEM_MAP_DEVICE(hw_cdev[i]->cd_id, &base);
#else
		/* Backwards compatible way:pass cd_id as uint32_t
		 * and &base is cast to (void **) for backwards compatibility.
		 */
		ret = SYSTEM_MAP_DEVICE((uint32_t)hw_cdev[i]->cd_id, (void **)&base);
#endif /* CCC_SYSTEM_MAP_DEVICE_ARGUMENT_NO_CASTING */

		if (NO_ERROR != ret) {
			CCC_ERROR_MESSAGE("Mapping device %s (%u) failed: %d\n",
					  hw_cdev[i]->cd_name, i, ret);
			break;
		}

		hw_cdev[i]->cd_base = base;
		if (CCC_BASE_WHEN_DEVICE_NOT_PRESENT != base) {
			hw_cdev[i]->cd_state = SE_CDEV_STATE_ACTIVE;
		}
	}
	CCC_ERROR_CHECK(ret);

#if HAVE_NO_STATIC_DATA_INITIALIZER
	static_initialize_nonzero();
#endif

	tegra_se_hw_initialized = SYSTEM_INITIALIZED;

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
	CCC_ERROR_MESSAGE("CCC measuring engine peformance (engine go => engine done)\n");
#endif

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
	CCC_ERROR_MESSAGE("CCC measuring operation context peformance (init start => dofinal done)\n");
#endif

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_MCP)
	CCC_ERROR_MESSAGE("CCC tracking waypoint timings\n");
#endif

#if TEGRA_MEASURE_MEMORY_USAGE
	CCC_ERROR_MESSAGE("CCC measuring heap consumption\n");
#endif

fail:
	return ret;
}

static const engine_t *engine_select(engine_class_t eclass, engine_id_t engine_hint)
{
	const engine_t *selected = NULL;
	uint32_t eindex = 0U;
	const uint32_t asize = sizeof_u32(ccc_engine);

	for (; eindex < (asize / sizeof_u32(engine_t)); eindex++) {
		const engine_t *ep = &ccc_engine[eindex];

		/* If engine supports the operation class =>
		 * see if it is active
		 */
		if ((ep->e_class & eclass) != 0U) {
#if defined(CCC_USING_SOFT_ENGINE)
			if (CCC_ENGINE_SOFT == ep->e_id) {
				if (NULL == selected) {
					selected = ep;
					continue;
				}
			}
#endif
			if (SE_CDEV_STATE_ACTIVE == ep->e_dev->cd_state) {
				/*
				 * by default select first engine that matches
				 * unless we have HW replacing SW...
				 */
				if (NULL == selected) {
					selected = ep;
				}

#if defined(CCC_USING_SOFT_ENGINE)
				if (CCC_ENGINE_SOFT == selected->e_id) {
					selected = ep;
				}
#endif

				/* Caller may prefer some other engine */
				if ((CCC_ENGINE_ANY == engine_hint) ||
				    (ep->e_id == engine_hint)) {
					selected = ep;
					break;
				}
			}
		}
	}

	return selected;
}

status_t ccc_select_engine(const engine_t **eng_p, engine_class_t eclass, engine_id_t engine_hint)
{
	status_t ret = NO_ERROR;
	const engine_t *selected = NULL;

	LTRACEF("entry\n");

	if (NULL == eng_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	selected = engine_select(eclass, engine_hint);
	*eng_p = selected;

#if SE_RD_DEBUG
	if (NULL == selected) {
		LTRACEF("No active engine class 0x%x found\n", eclass);
	} else {
		LTRACEF("Selected %s device %s\n",
			selected->e_dev->cd_name, selected->e_name);
	}
#endif

	if (NULL == selected) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_FOUND);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t ccc_select_device_engine(se_cdev_id_t device_id, const engine_t **eng_p,
				  engine_class_t eclass)
{
	status_t ret = NO_ERROR;
	const engine_t *selected = NULL;
	uint32_t eindex = 0U;
	const uint32_t asize = sizeof_u32(ccc_engine);

	LTRACEF("entry\n");

	if (NULL == eng_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	for (; eindex < (asize / sizeof_u32(engine_t)); eindex++) {
		const engine_t *ep = &ccc_engine[eindex];

		/* If engine supports the operation class => see if it is active */
		if ((ep->e_class & eclass) != 0U) {

			/* Soft engines do not match any device */
			if (CCC_ENGINE_SOFT != ep->e_id) {

				/* Consider an active engine only if
				 * it is part of the given device
				 */
				if ((device_id == ep->e_dev->cd_id) &&
				    (SE_CDEV_STATE_ACTIVE == ep->e_dev->cd_state)) {
					/*
					 * Select first HW engine that matches
					 * active device and class.
					 */
					selected = ep;
					break;
				}
			}
		}
	}

	*eng_p = selected;

#if SE_RD_DEBUG
	if (NULL == selected) {
		LTRACEF("No active device id %u found for engine class 0x%x\n", device_id, eclass);
	} else {
		LTRACEF("Selected %s device engine %s\n",
			selected->e_dev->cd_name, selected->e_name);
	}
#endif

	if (NULL == selected) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_FOUND);
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Production code version of non-tracing register read function when
 * not inlined Inlined versions in tegra_cdev.h
 */
#if CCC_INLINE_REGISTER_ACCESS == 0
/**
 * @brief not-inlined function for engine register reads.
 *
 * @param eng engine
 * @param reg register offset from base address
 * @return 32 bit register value
 */
uint32_t tegra_engine_get_reg_normal(const engine_t *eng, uint32_t reg)
{
	uint32_t reg1 = reg;

	if (BOOL_IS_TRUE(eng->e_do_remap) &&
	    (eng->e_remap_range.rm_start <= reg1) &&
	    (reg1 <= eng->e_remap_range.rm_end)) {
		uint32_t reg_offset = reg1 - eng->e_remap_range.rm_start;
		/* CERT INT30-C: Avoid integer overflow */
		if (eng->e_regs_begin <= (CCC_UINT_MAX - reg_offset)) {
			reg1 = eng->e_regs_begin + reg_offset;
		}
	}

	return ccc_device_get_reg(eng->e_dev, reg1);
}

/**
 * @brief not-inlined function for engine register writes.
 *
 * @param eng engine
 * @param reg register offset from base address
 * @param data 32 bit data to write
 */
void tegra_engine_set_reg_normal(const engine_t *eng, uint32_t reg, uint32_t data)
{
	uint32_t reg1 = reg;

	if (BOOL_IS_TRUE(eng->e_do_remap) &&
	    (eng->e_remap_range.rm_start <= reg1) &&
	    (reg1 <= eng->e_remap_range.rm_end)) {
		uint32_t reg_offset = reg1 - eng->e_remap_range.rm_start;
		/* CERT INT30-C: Avoid integer overflow */
		if (eng->e_regs_begin <= (CCC_UINT_MAX - reg_offset)) {
			reg1 = eng->e_regs_begin + reg_offset;
		}
	}

	ccc_device_set_reg(eng->e_dev, reg1, data);
}
#endif /* CCC_INLINE_REGISTER_ACCESS == 0 */

#if TRACE_REG_OPS

/* R&D functions for register operation tracing and logging.
 * Never enable these in production code.
 */
#if SE_RD_DEBUG == 0
#error "Can not TRACE_REG_OPS without SE_RD_DEBUG"
#endif

#ifndef LOG_REG_TRACE
#define LOG_REG_TRACE LOG_ALWAYS
#endif

/* VDK has severe output line length restrictions => need to
 * split lines or it drops output on the floor.
 */
#if CCC_SOC_FAMILY_ON(CCC_SOC_ON_VDK)
#define LOG_REG_READ32(eng,reg_s,fun_s,line_i)				\
	{ LOG_REG_TRACE("[%s:%u]\n", fun_s, line_i);			\
	  LOG_REG_TRACE("R32[%u](reg=0x%x %s)\n",			\
			(eng)->e_id, i__reg, (reg_s));			\
	  LOG_REG_TRACE("  -> (0x%0x)\n", i__data);			\
	}

#define LOG_REG_WRITE32(eng,reg_s,fun_s,line_i)				\
	{ LOG_REG_TRACE("[%s:%u]\n", fun_s, line_i);			\
	  LOG_REG_TRACE("W32[%u](r=0x%x %s)\n",				\
			(eng)->e_id, i__reg, (reg_s));			\
	  LOG_REG_TRACE(" = (0x%0x)\n", i__data);			\
	}
#else
#define LOG_REG_READ32(eng,reg_s,fun_s,line_i)				\
	LOG_REG_TRACE("[%s:%u] READ32[%u](reg=0x%x %s) -> %0u (0x%0x)\n",	\
		      fun_s, line_i, (eng)->e_id, i__reg, (reg_s), i__data, i__data)

#define LOG_REG_WRITE32(eng,reg_s,fun_s,line_i)				\
	LOG_REG_TRACE("[%s:%u] WRITE32[%u](reg=0x%x %s) = %0u (0x%0x)\n",	\
		      fun_s, line_i, (eng)->e_id, i__reg, (reg_s), i__data, i__data)
#endif /* SOC on VDK */

/**
 * @def tegra_engine_set_reg_trace_log(engine, reg, data)
 * @brief These macros are for heavy R&D only.
 *
 * Write engine register with data; tracing.
 *
 * Never use these on any code that deals with secret values.
 */
void tegra_engine_set_reg_trace_log(const engine_t *engine,
				    uint32_t reg,
				    uint32_t data,
				    const char *rname,
				    const char *fun_s,
				    uint32_t line_i)
{
	uint32_t i__reg = reg;
	uint32_t i__data = data;

	if (BOOL_IS_TRUE(engine->e_do_remap) &&
	    (engine->e_remap_range.rm_start <= i__reg) &&
	    (i__reg <= (engine->e_remap_range.rm_end))) {
		i__reg = engine->e_regs_begin + (i__reg - engine->e_remap_range.rm_start);
	}

	if (NULL != engine->e_dev->cd_base) {
		ccc_device_set_reg(engine->e_dev, i__reg, i__data);
	} else {
#if CCC_DEVICE_BASE_CAN_BE_NULL
		/* If a subsystem has the device base address set NULL
		 * we have to allow access to such a device.
		 */
		ccc_device_set_reg(engine->e_dev, i__reg, i__data);
#else
		LOG_ERROR("[%s:%u] %s base address undefined -- write ignored\n",
			  fun_s, line_i, engine->e_dev->cd_name);
#endif
	}

	LOG_REG_WRITE32(engine, rname, fun_s, line_i);
}

/**
 * @def tegra_engine_get_reg_trace_log(engine, reg)
 * @brief These macros are for heavy R&D only.
 *
 * Read engine register; tracing.
 *
 * Never use these on any code that deals with secret values.
 */
uint32_t tegra_engine_get_reg_trace_log(const engine_t *engine,
					uint32_t reg,
					const char *rname,
					const char *fun_s,
					uint32_t line_i)
{
	uint32_t i__reg = (reg);
	uint32_t i__data = 0U;

	if (BOOL_IS_TRUE(engine->e_do_remap) &&
	    (engine->e_remap_range.rm_start <= i__reg) &&
	    (i__reg <= engine->e_remap_range.rm_end)) {
		i__reg = engine->e_regs_begin + (i__reg - engine->e_remap_range.rm_start);
	}

	if (NULL != engine->e_dev->cd_base) {
		i__data = ccc_device_get_reg(engine->e_dev, i__reg);
	} else {
#if CCC_DEVICE_BASE_CAN_BE_NULL
		/* If a subsystem has the device base address set NULL
		 * we have to allow access to such a device.
		 */
		i__data = ccc_device_get_reg(engine->e_dev, i__reg);
#else
		LOG_ERROR("[%s:%u] %s base address undefined -- read ignored\n",
			  fun_s, line_i, engine->e_dev->cd_name);
#endif
	}

	LOG_REG_READ32(engine, rname, fun_s, line_i);

	return i__data;
}
#endif /* TRACE_REG_OPS */
