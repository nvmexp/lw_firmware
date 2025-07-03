/*
 * Copyright (c) 2019-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/* This file contains call-switches used after getting rid of
 * function pointers in the engine switch table.
 */
#include <crypto_system_config.h>

#include <crypto_ops.h>

#if defined(CCC_SOC_WITH_LWPKA) && CCC_SOC_WITH_LWPKA

#if HAVE_LWPKA_RSA
#include <lwpka/tegra_lwpka_rsa.h>
#endif
#if HAVE_LWPKA_ECC
#include <lwpka/tegra_lwpka_ec.h>

#if HAVE_LWPKA11_SUPPORT
#include <lwpka/lwpka_ec_ks_ops.h>
#endif

#endif /* HAVE_LWPKA_ECC */

#if HAVE_LWPKA_MODULAR
#include <lwpka/tegra_lwpka_mod.h>
#endif
#if HAVE_LWRNG_DRNG
#include <tegra_lwrng.h>
#endif

#else

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

#endif /* defined(CCC_SOC_WITH_LWPKA) && CCC_SOC_WITH_LWPKA */

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if 0
// XXX TODO: Check if makes sense to call the tag handlers e.g.
// with the following macro in switch statements?
// like this =>	FUNTAG_CASE_CALL(ft_aes,engine_aes_process_block_locked);
//
#define FUNTAG_CASE_CALL(pre,fun) \
	case FUNTAG(pre,fun): ret = fun(input_params, econtext); break
#endif // 0

#if HAVE_SE_AES
static inline status_t engine_call_ft_aes(enum ft_aes_e efun,
					  struct se_data_params *input_params,
					  struct se_engine_aes_context *econtext)
{
	status_t ret = ERR_FAULT;

	LTRACEF("entry\n");

	switch (efun) {
	case ft_aes_none:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;

	case ft_aes_engine_aes_process_block_locked:
		ret = engine_aes_process_block_locked(input_params, econtext);
		break;

#if HAVE_CMAC_AES
	case ft_aes_engine_cmac_process_block_locked:
		ret = engine_cmac_process_block_locked(input_params, econtext);
		break;
#endif

#if HAVE_SE_KAC_KEYOP
	case ft_aes_engine_aes_keyop_process_locked:
		ret = engine_aes_keyop_process_locked(input_params, econtext);
		break;
#endif

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_AES */

static inline status_t engine_call_ft_sha(enum ft_sha_e efun,
					  struct se_data_params *input_params,
					  struct se_engine_sha_context *econtext)
{
	status_t ret = ERR_FAULT;

	LTRACEF("entry\n");

	switch (efun) {
	case ft_sha_none:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;

	case ft_sha_engine_sha_process_block_locked:
		ret = engine_sha_process_block_locked(input_params, econtext);
		break;

#if HAVE_SE_KAC_KDF || HAVE_HW_HMAC_SHA
	case ft_sha_engine_sha_keyop_process_locked:
		ret = engine_sha_keyop_process_locked(input_params, econtext);
		break;
#endif

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if defined(CCC_SOC_WITH_LWPKA) && CCC_SOC_WITH_LWPKA

/* LWPKA versions */
static inline status_t engine_call_ft_m_lock(enum ft_m_lock_e efun,
					     const struct engine_s *engine)
{
	status_t ret = ERR_FAULT;

	LTRACEF("entry\n");

	switch (efun) {
	case ft_m_lock_none:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;

	case ft_m_lock_tegra_se0_mutex_lock:
		ret = tegra_se0_mutex_lock(engine);
		break;

#if HAVE_LWPKA_RSA || HAVE_LWPKA_ECC
	case ft_m_lock_lwpka_acquire_mutex:
		ret = lwpka_acquire_mutex(engine);
		break;
#endif

#if HAVE_LWRNG_DRNG
	case ft_m_lock_lwrng_acquire_mutex:
		ret = lwrng_acquire_mutex(engine);
		break;
#endif

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Mutex unlock returns void for hysteric raisins.
 */
static inline void engine_call_ft_m_unlock(enum ft_m_unlock_e efun,
					   const struct engine_s *engine)
{
	LTRACEF("entry\n");

	switch (efun) {
	case ft_m_unlock_tegra_se0_mutex_release:
		tegra_se0_mutex_release(engine);
		break;

#if HAVE_LWPKA_RSA || HAVE_LWPKA_ECC
	case ft_m_unlock_lwpka_release_mutex:
		lwpka_release_mutex(engine);
		break;
#endif

#if HAVE_LWRNG_DRNG
	case ft_m_unlock_lwrng_release_mutex:
		lwrng_release_mutex(engine);
		break;
#endif

	default:
		/* nothing */
		break;
	}

	LTRACEF("exit\n");
}

#if HAVE_LWPKA_RSA
static inline status_t engine_call_ft_rsa(enum ft_rsa_e efun,
					  struct se_data_params *input_params,
					  struct se_engine_rsa_context *econtext)
{
	status_t ret = ERR_FAULT;

	LTRACEF("entry\n");

	switch (efun) {
	case ft_rsa_none:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;

	case ft_rsa_engine_lwpka_rsa_modular_exp_locked:
		ret = engine_lwpka_rsa_modular_exp_locked(input_params, econtext);
		break;

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_LWPKA_RSA */

#if CCC_WITH_DRNG
static inline status_t engine_call_ft_rng(enum ft_rng_e efun,
					  struct se_data_params *input_params,
					  const struct se_engine_rng_context *econtext)
{
	status_t ret = ERR_FAULT;

	LTRACEF("entry\n");

	switch (efun) {
	case ft_rng_none:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;

#if HAVE_SE_GENRND
	case ft_rng_engine_genrnd_generate_locked:
		ret = engine_genrnd_generate_locked(input_params, econtext);
		break;
#endif

#if HAVE_LWRNG_DRNG
	case ft_rng_engine_lwrng_drng_generate_locked:
		ret = engine_lwrng_drng_generate_locked(input_params, econtext);
		break;
#endif

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_DRNG */

#if HAVE_LWPKA_ECC
static inline status_t engine_call_ft_ec(enum ft_ec_e efun,
					 struct se_data_params *input_params,
					 struct se_engine_ec_context *econtext)
{
	status_t ret = ERR_FAULT;

	LTRACEF("entry\n");

	switch (efun) {
	case ft_ec_none:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;

	case ft_ec_engine_lwpka_ec_point_mult_locked:
		ret = engine_lwpka_ec_point_mult_locked(input_params, econtext);
		break;

	case ft_ec_engine_lwpka_ec_point_verify_locked:
		ret = engine_lwpka_ec_point_verify_locked(input_params, econtext);
		break;

	case ft_ec_engine_lwpka_ec_shamir_trick_locked:
		ret = engine_lwpka_ec_shamir_trick_locked(input_params, econtext);
		break;

#if HAVE_LWPKA_X25519
	case ft_ec_engine_lwpka_montgomery_point_mult_locked:
		ret = engine_lwpka_montgomery_point_mult_locked(input_params, econtext);
		break;
#endif

#if HAVE_LWPKA_ED25519 || HAVE_LWPKA_ED448
	case ft_ec_engine_lwpka_edwards_point_mult_locked:
		ret = engine_lwpka_edwards_point_mult_locked(input_params, econtext);
		break;

	case ft_ec_engine_lwpka_edwards_point_verify_locked:
		ret = engine_lwpka_edwards_point_verify_locked(input_params, econtext);
		break;

	case ft_ec_engine_lwpka_edwards_shamir_trick_locked:
		ret = engine_lwpka_edwards_shamir_trick_locked(input_params, econtext);
		break;
#endif

#if HAVE_LWPKA11_SUPPORT
	case ft_ec_engine_lwpka_ec_ks_ecdsa_sign_locked:
		ret = engine_lwpka_ec_ks_ecdsa_sign_locked(input_params, econtext);
		break;
#if HAVE_LWPKA11_ECKS_KEYOPS
	case ft_ec_engine_lwpka_ec_ks_keyop_locked:
		ret = engine_lwpka_ec_ks_keyop_locked(input_params, econtext);
		break;
#endif
#endif /* HAVE_LWPKA11_SUPPORT */

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_LWPKA_ECC */

#if CCC_WITH_MODULAR
static inline status_t engine_call_ft_mod(enum ft_mod_e efun,
					  const engine_t *engine,
					  const te_mod_params_t *params)
{
	status_t ret = ERR_FAULT;

	LTRACEF("entry\n");

	switch (efun) {
	case ft_mod_none:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;

	case ft_mod_engine_lwpka_modular_multiplication_locked:
		ret = engine_lwpka_modular_multiplication_locked(engine, params);
		break;

	case ft_mod_engine_lwpka_modular_addition_locked:
		ret = engine_lwpka_modular_addition_locked(engine, params);
		break;

	case ft_mod_engine_lwpka_modular_subtraction_locked:
		ret = engine_lwpka_modular_subtraction_locked(engine, params);
		break;

	case ft_mod_engine_lwpka_modular_reduction_locked:
		ret = engine_lwpka_modular_reduction_locked(engine, params);
		break;

#if CCC_WITH_MODULAR_DIVISION
	case ft_mod_engine_lwpka_modular_division_locked:
		ret = engine_lwpka_modular_division_locked(engine, params);
		break;
#endif

	case ft_mod_engine_lwpka_modular_ilwersion_locked:
		ret = engine_lwpka_modular_ilwersion_locked(engine, params);
		break;

	case ft_mod_engine_lwpka_modular_dp_reduction_locked:
		ret = engine_lwpka_modular_dp_reduction_locked(engine, params);
		break;

#if HAVE_GEN_MULTIPLY
	case ft_mod_engine_lwpka_gen_mult_locked:
		ret = engine_lwpka_gen_mult_locked(engine, params);
		break;

	case ft_mod_engine_lwpka_gen_mult_mod_locked:
		ret = engine_lwpka_gen_mult_mod_locked(engine, params);
		break;
#endif

	case ft_mod_engine_lwpka_modular_exponentiation_locked:
		ret = engine_lwpka_modular_exponentiation_locked(engine, params);
		break;

	case ft_mod_engine_lwpka_modular_square_locked:
		ret = engine_lwpka_modular_square_locked(engine, params);
		break;

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_MODULAR */

#else

/* PKA1 versions */
static inline status_t engine_call_ft_m_lock(enum ft_m_lock_e efun,
					     const struct engine_s *engine)
{
	status_t ret = ERR_FAULT;

	LTRACEF("entry\n");

	switch (efun) {
	case ft_m_lock_none:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;

	case ft_m_lock_tegra_se0_mutex_lock:
		ret = tegra_se0_mutex_lock(engine);
		break;

#if HAVE_RNG1_DRNG
	case ft_m_lock_rng1_acquire_mutex:
		ret = rng1_acquire_mutex(engine);
		break;
#endif

#if HAVE_PKA1_RSA || HAVE_PKA1_ECC || HAVE_PKA1_TRNG
	case ft_m_lock_pka1_acquire_mutex:
		ret = pka1_acquire_mutex(engine);
		break;
#endif

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Mutex unlock returns void for hysteric raisins.
 */
static inline void engine_call_ft_m_unlock(enum ft_m_unlock_e efun,
					   const struct engine_s *engine)
{
	LTRACEF("entry\n");

	switch (efun) {
	case ft_m_unlock_tegra_se0_mutex_release:
		tegra_se0_mutex_release(engine);
		break;

#if HAVE_RNG1_DRNG
	case ft_m_unlock_rng1_release_mutex:
		rng1_release_mutex(engine);
		break;
#endif

#if HAVE_PKA1_RSA || HAVE_PKA1_ECC || HAVE_PKA1_TRNG
	case ft_m_unlock_pka1_release_mutex:
		pka1_release_mutex(engine);
		break;
#endif

	default:
		/* nothing */
		break;
	}

	LTRACEF("exit\n");
}

static inline status_t engine_call_ft_rsa(enum ft_rsa_e efun,
					  struct se_data_params *input_params,
					  struct se_engine_rsa_context *econtext)
{
	status_t ret = ERR_FAULT;

	LTRACEF("entry\n");

	switch (efun) {
	case ft_rsa_none:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;

#if HAVE_SE_RSA
	case ft_rsa_engine_rsa_modular_exp_locked:
		ret = engine_rsa_modular_exp_locked(input_params, econtext);
		break;
#endif

#if HAVE_PKA1_RSA
	case ft_rsa_engine_pka1_rsa_modular_exp_locked:
		ret = engine_pka1_rsa_modular_exp_locked(input_params, econtext);
		break;
#endif

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if CCC_WITH_DRNG || HAVE_PKA1_TRNG
static inline status_t engine_call_ft_rng(enum ft_rng_e efun,
					  struct se_data_params *input_params,
					  const struct se_engine_rng_context *econtext)
{
	status_t ret = ERR_FAULT;

	LTRACEF("entry\n");

	switch (efun) {
	case ft_rng_none:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;

#if HAVE_SE_RANDOM
	case ft_rng_engine_rng0_generate_locked:
		ret = engine_rng0_generate_locked(input_params, econtext);
		break;
#endif

#if HAVE_RNG1_DRNG
	case ft_rng_engine_rng1_drng_generate_locked:
		ret = engine_rng1_drng_generate_locked(input_params, econtext);
		break;
#endif

#if HAVE_PKA1_TRNG
	case ft_rng_engine_pka1_trng_generate_locked:
		ret = engine_pka1_trng_generate_locked(input_params, econtext);
		break;
#endif

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_DRNG || HAVE_PKA1_TRNG */

static inline status_t engine_call_ft_ec(enum ft_ec_e efun,
					 struct se_data_params *input_params,
					 struct se_engine_ec_context *econtext)
{
	status_t ret = ERR_FAULT;

	LTRACEF("entry\n");

	switch (efun) {
	case ft_ec_none:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;

	case ft_ec_engine_pka1_ec_point_mult_locked:
		ret = engine_pka1_ec_point_mult_locked(input_params, econtext);
		break;

#if HAVE_PKA1_ECC_POINT_DOUBLE
	case ft_ec_engine_pka1_ec_point_double_locked:
		ret = engine_pka1_ec_point_double_locked(input_params, econtext);
		break;
#endif

#if HAVE_PKA1_ECC_POINT_ADD
	case ft_ec_engine_pka1_ec_point_add_locked:
		ret = engine_pka1_ec_point_add_locked(input_params, econtext);
		break;
#endif

	case ft_ec_engine_pka1_ec_point_verify_locked:
		ret = engine_pka1_ec_point_verify_locked(input_params, econtext);
		break;

	case ft_ec_engine_pka1_ec_shamir_trick_locked:
		ret = engine_pka1_ec_shamir_trick_locked(input_params, econtext);
		break;

#if HAVE_PKA1_X25519 && HAVE_ELLIPTIC_20
	case ft_ec_engine_pka1_c25519_point_mult_locked:
		ret = engine_pka1_c25519_point_mult_locked(input_params, econtext);
		break;
#endif

#if HAVE_PKA1_ED25519
	case ft_ec_engine_pka1_ed25519_point_mult_locked:
		ret = engine_pka1_ed25519_point_mult_locked(input_params, econtext);
		break;

#if HAVE_PKA1_ED25519_POINT_DOUBLE
	case ft_ec_engine_pka1_ed25519_point_double_locked:
		ret = engine_pka1_ed25519_point_double_locked(input_params, econtext);
		break;
#endif

#if HAVE_PKA1_ED25519_POINT_ADD
	case ft_ec_engine_pka1_ed25519_point_add_locked:
		ret = engine_pka1_ed25519_point_add_locked(input_params, econtext);
		break;
#endif

	case ft_ec_engine_pka1_ed25519_point_verify_locked:
		ret = engine_pka1_ed25519_point_verify_locked(input_params, econtext);
		break;

	case ft_ec_engine_pka1_ed25519_shamir_trick_locked:
		ret = engine_pka1_ed25519_shamir_trick_locked(input_params, econtext);
		break;
#endif /* HAVE_PKA1_ED25519 */

#if HAVE_P521
	case ft_ec_engine_pka1_ec_p521_point_mult_locked:
		ret = engine_pka1_ec_p521_point_mult_locked(input_params, econtext);
		break;

#if HAVE_PKA1_P521_POINT_DOUBLE
	case ft_ec_engine_pka1_ec_p521_point_double_locked:
		ret = engine_pka1_ec_p521_point_double_locked(input_params, econtext);
		break;
#endif

#if HAVE_PKA1_P521_POINT_ADD
	case ft_ec_engine_pka1_ec_p521_point_add_locked:
		ret = engine_pka1_ec_p521_point_add_locked(input_params, econtext);
		break;
#endif

	case ft_ec_engine_pka1_ec_p521_point_verify_locked:
		ret = engine_pka1_ec_p521_point_verify_locked(input_params, econtext);
		break;

	case ft_ec_engine_pka1_ec_p521_shamir_trick_locked:
		ret =engine_pka1_ec_p521_shamir_trick_locked(input_params, econtext);
		break;
#endif /* HAVE_P521 */

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if CCC_WITH_MODULAR
static inline status_t engine_call_ft_mod(enum ft_mod_e efun,
					  const engine_t *engine,
					  const te_mod_params_t *params)
{
	status_t ret = ERR_FAULT;

	LTRACEF("entry\n");

	switch (efun) {
	case ft_mod_none:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;

	case ft_mod_engine_pka1_modular_multiplication_locked:
		ret = engine_pka1_modular_multiplication_locked(engine, params);
		break;

	case ft_mod_engine_pka1_modular_addition_locked:
		ret = engine_pka1_modular_addition_locked(engine, params);
		break;

	case ft_mod_engine_pka1_modular_subtraction_locked:
		ret = engine_pka1_modular_subtraction_locked(engine, params);
		break;

	case ft_mod_engine_pka1_modular_reduction_locked:
		ret = engine_pka1_modular_reduction_locked(engine, params);
		break;

#if CCC_WITH_MODULAR_DIVISION
	case ft_mod_engine_pka1_modular_division_locked:
		ret = engine_pka1_modular_division_locked(engine, params);
		break;
#endif

	case ft_mod_engine_pka1_modular_ilwersion_locked:
		ret = engine_pka1_modular_ilwersion_locked(engine, params);
		break;

#if CCC_WITH_BIT_SERIAL_REDUCTION
	case ft_mod_engine_pka1_bit_serial_reduction_locked:
		ret = engine_pka1_bit_serial_reduction_locked(engine, params);
		break;
#endif

	case ft_mod_engine_pka1_bit_serial_double_precision_reduction_locked:
		ret = engine_pka1_bit_serial_double_precision_reduction_locked(engine, params);
		break;

#if HAVE_GEN_MULTIPLY
	case ft_mod_engine_pka1_gen_mult_locked:
		ret = engine_pka1_gen_mult_locked(engine, params);
		break;

	case ft_mod_engine_pka1_gen_mult_mod_locked:
		ret = engine_pka1_gen_mult_mod_locked(engine, params);
		break;
#endif

#if HAVE_PKA1_ED25519 && HAVE_ELLIPTIC_20
	case ft_mod_engine_pka1_c25519_mod_exp_locked:
		ret = engine_pka1_c25519_mod_exp_locked(engine, params);
		break;

	case ft_mod_engine_pka1_c25519_mod_square_locked:
		ret = engine_pka1_c25519_mod_square_locked(engine, params);
		break;

	case ft_mod_engine_pka1_c25519_mod_mult_locked:
		ret = engine_pka1_c25519_mod_mult_locked(engine, params);
		break;
#endif

#if HAVE_P521
	case ft_mod_engine_pka1_modular_m521_multiplication_locked:
		ret = engine_pka1_modular_m521_multiplication_locked(engine, params);
		break;

	case ft_mod_engine_pka1_modular_multiplication521_locked:
		ret = engine_pka1_modular_multiplication521_locked(engine, params);
		break;
#endif

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_MODULAR */
#endif /* defined(CCC_SOC_WITH_LWPKA) && CCC_SOC_WITH_LWPKA */

#if HAVE_SE_AES && HAVE_SE_ASYNC_AES
static inline status_t engine_call_ft_aes_async(enum ft_aes_async_e efun,
						async_aes_ctx_t *ac)
{
	status_t ret = ERR_FAULT;

	LTRACEF("entry\n");

	switch (efun) {
	case ft_aes_async_none:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;

	case ft_aes_async_engine_aes_locked_async_start:
		ret = engine_aes_locked_async_start(ac);
		break;

	case ft_aes_async_engine_aes_locked_async_finish:
		ret = engine_aes_locked_async_finish(ac);
		break;

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static inline status_t engine_call_ft_aes_poll(enum ft_aes_poll_e efun,
					       const async_aes_ctx_t *ac,
					       bool *is_idle_p)
{
	status_t ret = ERR_FAULT;

	LTRACEF("entry\n");

	switch (efun) {
	case ft_aes_poll_none:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;

	case ft_aes_poll_engine_aes_locked_async_done:
		ret = engine_aes_locked_async_done(ac, is_idle_p);
		break;

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_AES && HAVE_SE_ASYNC_AES */

#if HAVE_SE_SHA && HAVE_SE_ASYNC_SHA
static inline status_t engine_call_ft_sha_async(enum ft_sha_async_e efun,
						async_sha_ctx_t *ac)
{
	status_t ret = ERR_FAULT;

	LTRACEF("entry\n");

	switch (efun) {
	case ft_sha_async_none:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;

	case ft_sha_async_engine_sha_locked_async_start:
		ret = engine_sha_locked_async_start(ac);
		break;

	case ft_sha_async_engine_sha_locked_async_finish:
		ret = engine_sha_locked_async_finish(ac);
		break;

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static inline status_t engine_call_ft_sha_poll(enum ft_sha_poll_e efun,
					       const async_sha_ctx_t *ac,
					       bool *is_idle_p)
{
	status_t ret = ERR_FAULT;

	LTRACEF("entry\n");

	switch (efun) {
	case ft_sha_poll_none:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;

	case ft_sha_poll_engine_sha_locked_async_done:
		ret = engine_sha_locked_async_done(ac, is_idle_p);
		break;

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_SHA && HAVE_SE_ASYNC_SHA */

/* CDEV method calls without function pointers.
 *
 * These functions are called via CCC_ENGINE_SWITCH_*() macros and
 * all of these just call the engine code tagged in the engine object
 * e_proc fields.
 *
 * In this setup each e_proc.p_<method> field is a engine function tag
 * or (uint32_t)0 if not defined for this engine.
 *
 * The function associated with the tag in the engine table is called
 * via the engine_call_ft_XXX() function.
 */
#if HAVE_SE_SHA
status_t engine_method_sha_econtext_locked(const engine_t *engine,
					   struct se_data_params *input,
					   struct se_engine_sha_context *econtext,
					   enum ecall_sha_variant_e evariant)
{
	status_t ret = NO_ERROR;
	enum ft_sha_e etag = ft_sha_none;

	LTRACEF("entry\n");

	if (NULL == engine) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (evariant) {
	case CDEV_ECALL_SHA:
		etag = engine->e_proc.p_sha;
		break;

#if HAVE_SE_KAC_KDF || HAVE_HW_HMAC_SHA
	case CDEV_ECALL_SHA_KEYOP:
		etag = engine->e_proc.p_sha_keyop;
		break;
#endif /* HAVE_SE_KAC_KDF || HAVE_HW_HMAC_SHA */

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	LTRACEF("Calling %s SHA variant 0x%x handler %u\n",
		engine->e_name, evariant, etag);

	ret = engine_call_ft_sha(etag, input, econtext);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_SHA */

#if HAVE_SE_AES
status_t engine_method_aes_econtext_locked(const engine_t *engine,
					   struct se_data_params *input,
					   struct se_engine_aes_context *econtext,
					   enum ecall_aes_variant_e evariant)
{
	status_t ret = NO_ERROR;
	enum ft_aes_e etag = ft_aes_none;

	LTRACEF("entry\n");

	if (NULL == engine) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (evariant) {
	case CDEV_ECALL_AES:
		etag = engine->e_proc.p_aes;
		break;

#if HAVE_CMAC_AES
	case CDEV_ECALL_AES_MAC:
		etag = engine->e_proc.p_cmac;
		break;
#endif

#if HAVE_SE_KAC_KEYOP
	case CDEV_ECALL_AES_KWUW:
		etag = engine->e_proc.p_aes_kwuw;
		break;
#endif

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	LTRACEF("Calling %s AES variant 0x%x handler %u\n",
		engine->e_name, evariant, etag);

	ret = engine_call_ft_aes(etag, input, econtext);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_AES */

#if CCC_WITH_RSA
status_t engine_method_rsa_exp_econtext_locked(const engine_t *engine,
					       struct se_data_params *input,
					       struct se_engine_rsa_context *econtext,
					       enum ecall_rsa_variant_e evariant)
{
	status_t ret = NO_ERROR;
	enum ft_rsa_e etag = ft_rsa_none;

	LTRACEF("entry\n");

	if (NULL == engine) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (evariant) {
	case CDEV_ECALL_RSA_EXP:
		etag = engine->e_proc.p_rsa_exp;
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	LTRACEF("Calling %s RSA variant 0x%x handler %u\n",
		engine->e_name, evariant, etag);

	ret = engine_call_ft_rsa(etag, input, econtext);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_RSA */

#if CCC_WITH_DRNG || HAVE_PKA1_TRNG
status_t engine_method_rng_econtext_locked(const engine_t *engine,
					   struct se_data_params *input,
					   const struct se_engine_rng_context *econtext,
					   enum ecall_rng_variant_e evariant)
{
	status_t ret = NO_ERROR;
	enum ft_rng_e etag = ft_rng_none;

	LTRACEF("entry: engine %s p_rng %u\n", engine->e_name, engine->e_proc.p_rng);

	if (NULL == engine) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (evariant) {
	case CDEV_ECALL_RNG:
		etag = engine->e_proc.p_rng;
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	LTRACEF("Calling %s RNG variant 0x%x handler %u\n",
		engine->e_name, evariant, etag);

	ret = engine_call_ft_rng(etag, input, econtext);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_DRNG || HAVE_PKA1_TRNG */

#if HAVE_SE_SHA && HAVE_SE_ASYNC_SHA
status_t engine_method_async_sha_check_actx_locked(const engine_t *engine,
						   const struct async_sha_ctx_s *actx,
						   bool *is_done,
						   enum ecall_async_variant_e evariant)
{
	status_t ret = NO_ERROR;
	enum ft_sha_poll_e etag = ft_sha_poll_none;

	LTRACEF("entry\n");

	if ((NULL == engine) ||
	    (NULL == actx) ||
	    (CDEV_ECALL_ASYNC_CHECK != evariant) ||
	    (NULL == is_done)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}
	etag = engine->e_proc.p_sha_async_done;

	LTRACEF("Calling %s SHA POLL variant 0x%x handler %u\n",
		engine->e_name, evariant, etag);

	ret = engine_call_ft_sha_poll(etag, actx, is_done);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t engine_method_async_sha_actx_locked(const engine_t *engine,
					     struct async_sha_ctx_s *actx,
					     enum ecall_async_variant_e evariant)
{
	status_t ret = NO_ERROR;
	enum ft_sha_async_e etag = ft_sha_async_none;

	LTRACEF("entry\n");

	if ((NULL == engine) || (NULL == actx)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (evariant) {
	case CDEV_ECALL_ASYNC_START:
		etag = engine->e_proc.p_sha_async_start;
		break;

	case CDEV_ECALL_ASYNC_FINISH:
		etag = engine->e_proc.p_sha_async_finish;
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	LTRACEF("Calling %s SHA ASYNC variant 0x%x handler %u\n",
		engine->e_name, evariant, etag);

	ret = engine_call_ft_sha_async(etag, actx);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_SHA && HAVE_SE_ASYNC_SHA */

#if HAVE_SE_AES && HAVE_SE_ASYNC_AES
status_t engine_method_async_aes_check_actx_locked(const engine_t *engine,
						   const struct async_aes_ctx_s *actx,
						   bool *is_done,
						   enum ecall_async_variant_e evariant)
{
	status_t ret = NO_ERROR;
	enum ft_aes_poll_e etag = ft_aes_poll_none;

	LTRACEF("entry\n");

	if ((NULL == engine) ||
	    (NULL == actx) ||
	    (CDEV_ECALL_ASYNC_CHECK != evariant) ||
	    (NULL == is_done)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	etag = engine->e_proc.p_aes_async_done;

	LTRACEF("Calling %s AES POLL variant 0x%x handler %u\n",
		engine->e_name, evariant, etag);

	ret = engine_call_ft_aes_poll(etag, actx, is_done);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t engine_method_async_aes_actx_locked(const engine_t *engine,
					     struct async_aes_ctx_s *actx,
					     enum ecall_async_variant_e evariant)
{
	status_t ret = NO_ERROR;
	enum ft_aes_async_e etag = ft_aes_async_none;

	LTRACEF("entry\n");

	if ((NULL == engine) || (NULL == actx)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (evariant) {
	case CDEV_ECALL_ASYNC_START:
		etag = engine->e_proc.p_aes_async_start;
		break;

	case CDEV_ECALL_ASYNC_FINISH:
		etag = engine->e_proc.p_aes_async_finish;
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	LTRACEF("Calling %s AES ASYNC variant 0x%x handler %u\n",
		engine->e_name, evariant, etag);

	ret = engine_call_ft_aes_async(etag, actx);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_AES && HAVE_SE_ASYNC_AES */

#if CCC_WITH_ECC

status_t engine_method_ec_econtext_locked(const engine_t *engine,
					  struct se_data_params *input,
					  struct se_engine_ec_context *econtext,
					  enum ecall_ec_variant_e evariant)
{
	status_t ret = NO_ERROR;
	enum ft_ec_e etag = ft_ec_none;

	LTRACEF("entry\n");

	if (NULL == engine) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (evariant) {
	case CDEV_ECALL_EC_MULT:
		etag = engine->e_proc.p_ec_p_mult;
		break;

#if X_EC_POINT_ADD
	case CDEV_ECALL_EC_ADD:
		etag = engine->e_proc.p_ec_p_add;
		break;
#endif

#if X_EC_POINT_DOUBLE
	case CDEV_ECALL_EC_DOUBLE:
		etag = engine->e_proc.p_ec_p_double;
		break;
#endif

	case CDEV_ECALL_EC_VERIFY:
		etag = engine->e_proc.p_ec_p_verify;
		break;

	case CDEV_ECALL_EC_SHAMIR_TRICK:
		etag = engine->e_proc.p_ec_p_shamir_trick;
		break;

#if HAVE_LWPKA11_SUPPORT
	case CDEV_ECALL_EC_ECDSA_SIGN:
		etag = engine->e_proc.p_ec_ks_ecdsa_sign;
		break;

#if HAVE_LWPKA11_ECKS_KEYOPS
	case CDEV_ECALL_EC_KS_KEYOP:
		etag = engine->e_proc.p_ec_ks_keyop;
		break;
#endif

#endif /* HAVE_LWPKA11_SUPPORT */

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	LTRACEF("Calling %s EC variant 0x%x handler %u\n",
		engine->e_name, evariant, etag);

	/* Call the evariant selected EC function via etag
	 */
	ret = engine_call_ft_ec(etag, input, econtext);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_ECC */

#if CCC_WITH_MODULAR
static inline status_t get_mod_evariant_etag_rest(const engine_t *engine,
						  enum ecall_mod_variant_e evariant,
						  enum ft_mod_e *etag_p)
{
	status_t ret = NO_ERROR;
	enum ft_mod_e etag = ft_mod_none;

	LTRACEF("entry\n");

	switch (evariant) {
#if CCC_WITH_ED25519
	case CDEV_ECALL_MOD_C25519_EXP:
		etag = engine->e_proc.p_mod_c25519_exp;
		break;

	case CDEV_ECALL_MOD_C25519_SQR:
		etag = engine->e_proc.p_mod_c25519_sqr;
		break;

	case CDEV_ECALL_MOD_C25519_MULT:
		etag = engine->e_proc.p_mod_c25519_mult;
		break;
#endif /* CCC_WITH_ED25519 */

#if HAVE_P521
	case CDEV_ECALL_MOD_MULT_M521:
		etag = engine->e_proc.p_mod_mult_m521;
		break;

	case CDEV_ECALL_MOD_MULT_MONT521:
		etag = engine->e_proc.p_mod_mult_mont521;
		break;
#endif /* HAVE_P521 */

	case CDEV_ECALL_MOD_BIT_REDUCTION_DP:
		etag = engine->e_proc.p_mod_bit_reduction_dp;
		break;

#if HAVE_GEN_MULTIPLY
	case CDEV_ECALL_GEN_MULTIPLICATION:
		etag = engine->e_proc.p_gen_multiplication;
		break;

	case CDEV_ECALL_MOD_GEN_MULTIPLICATION:
		etag = engine->e_proc.p_mod_gen_multiplication;
		break;
#endif /* HAVE_GEN_MULTIPLY */

	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	*etag_p = etag;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static inline status_t get_mod_evariant_etag(const engine_t *engine,
					     enum ecall_mod_variant_e evariant,
					     enum ft_mod_e *etag_p)
{
	status_t ret = NO_ERROR;
	enum ft_mod_e etag = ft_mod_none;

	LTRACEF("entry\n");

	switch (evariant) {
	case CDEV_ECALL_MOD_MULTIPLICATION:
		etag = engine->e_proc.p_mod_multiplication;
		break;

	case CDEV_ECALL_MOD_ADDITION:
		etag = engine->e_proc.p_mod_addition;
		break;

	case CDEV_ECALL_MOD_SUBTRACTION:
		etag = engine->e_proc.p_mod_subtraction;
		break;

	case CDEV_ECALL_MOD_REDUCTION:
		etag = engine->e_proc.p_mod_reduction;
		break;

#if CCC_WITH_MODULAR_DIVISION
	case CDEV_ECALL_MOD_DIVISION:
		etag = engine->e_proc.p_mod_division;
		break;
#endif /* CCC_WITH_MODULAR_DIVISION */

	case CDEV_ECALL_MOD_ILWERSION:
		etag = engine->e_proc.p_mod_ilwersion;
		break;

#if CCC_WITH_BIT_SERIAL_REDUCTION
	case CDEV_ECALL_MOD_BIT_REDUCTION:
		etag = engine->e_proc.p_mod_bit_reduction;
		break;
#endif /* CCC_WITH_BIT_SERIAL_REDUCTION */

	default:
		ret = get_mod_evariant_etag_rest(engine, evariant, &etag);
		break;
	}
	CCC_ERROR_CHECK(ret);

	*etag_p = etag;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t engine_method_mod_params_locked(const engine_t *engine,
					 const struct te_mod_params_s *modparam,
					 enum ecall_mod_variant_e evariant)
{
	status_t ret = ERR_FAULT;
	enum ft_mod_e etag = ft_mod_none;

	LTRACEF("entry\n");

	if ((NULL == engine) || (NULL == modparam)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = get_mod_evariant_etag(engine, evariant, &etag);
	CCC_ERROR_CHECK(ret);

	LTRACEF("Calling %s MOD variant 0x%x handler %u\n",
		engine->e_name, evariant, etag);

	/* Call the evariant selected MOD function
	 */
	ret = engine_call_ft_mod(etag, engine, modparam);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_MODULAR */

status_t engine_method_mutex_acquire(const struct engine_s *engine)
{
	status_t ret = ERR_FAULT;

	LTRACEF("entry\n");

	if (NULL == engine) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = engine_call_ft_m_lock(engine->e_mutex.m_lock, engine);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

void engine_method_mutex_release(const struct engine_s *engine)
{
	LTRACEF("entry\n");

	if (NULL != engine) {
		engine_call_ft_m_unlock(engine->e_mutex.m_unlock, engine);
	}

	LTRACEF("exit\n");
}
