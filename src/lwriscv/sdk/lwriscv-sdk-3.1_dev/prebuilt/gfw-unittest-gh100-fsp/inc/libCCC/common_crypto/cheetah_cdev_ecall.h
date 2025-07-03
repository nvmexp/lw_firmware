/*
 * Copyright (c) 2019-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/**
 * @file   tegra_cdev_ecall.h
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2020
 *
 * Call engine handlers via the functions, tags and macros in tegra_cdev_ecall.[hc]
 *
 * If defined zero call the engine handlers via static const engine_t
 * ccc_engine[] table.
 */
#ifndef INCLUDED_CRYPTO_ENGINECALL_H
#define INCLUDED_CRYPTO_ENGINECALL_H

/**
 * @def FUNTAG(ftag, ft_fun)
 *
 * @short Use a FUNCTION TAG instead of function pointers
 * to call engine switch functions.
 *
 * The TAG is used by the engine call switch to call the
 * matching function, avoiding the function pointer.
 */
#define FUNTAG(ftag, ft_fun) ftag ## _ ## ft_fun

struct engine_s;

status_t engine_method_mutex_acquire(const struct engine_s *engine);
void engine_method_mutex_release(const struct engine_s *engine);

/**
 * @def DO_HW_MUTEX_ENGINE_LOCK(engine)
 *
 * @short Engine HW mutex lock call switch
 */
#define DO_HW_MUTEX_ENGINE_LOCK(engine) engine_method_mutex_acquire(engine)

/**
 * @def DO_HW_MUTEX_ENGINE_LOCK(engine)
 *
 * @short Engine HW mutex unlock call switch
 */
#define DO_HW_MUTEX_ENGINE_RELEASE(engine) engine_method_mutex_release(engine)

/**
 * @ingroup eng_switch_table
 * @defgroup eng_call_tags Engine switch layer tags for
 * calling engine handlers
 *
 * These are grouped by the function argument types: all group
 * member has identical argument types, distinct from other groups.
 */
/*@{*/
enum ft_aes_e {
	ft_aes_none,
	ft_aes_first,
	ft_aes_engine_aes_process_block_locked,
	ft_aes_engine_cmac_process_block_locked,
	ft_aes_engine_aes_keyop_process_locked,
	ft_aes_last,
};

enum ft_rng_e {
	ft_rng_none,
	ft_rng_first = 10,
	ft_rng_engine_genrnd_generate_locked,
	ft_rng_engine_lwrng_drng_generate_locked,
	ft_rng_engine_rng1_drng_generate_locked,
	ft_rng_engine_rng0_generate_locked,
	ft_rng_engine_pka1_trng_generate_locked,
	ft_rng_last,
};

enum ft_m_lock_e {
	ft_m_lock_none,
	ft_m_lock_first = 20,
	ft_m_lock_tegra_se0_mutex_lock,
	ft_m_lock_lwpka_acquire_mutex,
	ft_m_lock_lwrng_acquire_mutex,
	ft_m_lock_rng1_acquire_mutex,
	ft_m_lock_pka1_acquire_mutex,
	ft_m_lock_last,
};

enum ft_m_unlock_e {
	ft_m_unlock_none,
	ft_m_unlock_first = 30,
	ft_m_unlock_tegra_se0_mutex_release,
	ft_m_unlock_lwpka_release_mutex,
	ft_m_unlock_lwrng_release_mutex,
	ft_m_unlock_rng1_release_mutex,
	ft_m_unlock_pka1_release_mutex,
	ft_m_unlock_last,
};

enum ft_rsa_e {
	ft_rsa_none,
	ft_rsa_first = 40,
	ft_rsa_engine_rsa_modular_exp_locked,
	ft_rsa_engine_lwpka_rsa_modular_exp_locked,
	ft_rsa_engine_pka1_rsa_modular_exp_locked,
	ft_rsa_last,
};

enum ft_sha_e {
	ft_sha_none,
	ft_sha_first = 50,
	ft_sha_engine_sha_process_block_locked,
	ft_sha_engine_sha_keyop_process_locked,
	ft_sha_last,
};

enum ft_ec_e {
	ft_ec_none,
	ft_ec_first = 150,
	ft_ec_engine_lwpka_ec_point_mult_locked,
	ft_ec_engine_lwpka_ec_point_verify_locked,
	ft_ec_engine_lwpka_ec_shamir_trick_locked,
	ft_ec_engine_lwpka_montgomery_point_mult_locked,
	ft_ec_engine_lwpka_edwards_point_mult_locked,
	ft_ec_engine_lwpka_edwards_point_verify_locked,
	ft_ec_engine_lwpka_edwards_shamir_trick_locked,
	ft_ec_engine_pka1_ec_point_mult_locked,
	ft_ec_engine_pka1_ec_point_double_locked,
	ft_ec_engine_pka1_ec_point_add_locked,
	ft_ec_engine_pka1_ec_point_verify_locked,
	ft_ec_engine_pka1_ec_shamir_trick_locked,
	ft_ec_engine_pka1_c25519_point_mult_locked,
	ft_ec_engine_pka1_ed25519_point_mult_locked,
	ft_ec_engine_pka1_ed25519_point_double_locked,
	ft_ec_engine_pka1_ed25519_point_add_locked,
	ft_ec_engine_pka1_ed25519_point_verify_locked,
	ft_ec_engine_pka1_ed25519_shamir_trick_locked,
	ft_ec_engine_pka1_ec_p521_point_mult_locked,
	ft_ec_engine_pka1_ec_p521_point_double_locked,
	ft_ec_engine_pka1_ec_p521_point_add_locked,
	ft_ec_engine_pka1_ec_p521_point_verify_locked,
	ft_ec_engine_pka1_ec_p521_shamir_trick_locked,
#if HAVE_LWPKA11_SUPPORT
	ft_ec_engine_lwpka_ec_ks_ecdsa_sign_locked,
	ft_ec_engine_lwpka_ec_ks_keyop_locked,
#endif
	ft_ec_last,
};

enum ft_mod_e {
	ft_mod_none,
	ft_mod_first = 70,
	ft_mod_engine_lwpka_modular_multiplication_locked,
	ft_mod_engine_lwpka_modular_addition_locked,
	ft_mod_engine_lwpka_modular_subtraction_locked,
	ft_mod_engine_lwpka_modular_reduction_locked,
	ft_mod_engine_lwpka_modular_division_locked,
	ft_mod_engine_lwpka_modular_ilwersion_locked,
	ft_mod_engine_lwpka_modular_dp_reduction_locked,
	ft_mod_engine_lwpka_gen_mult_locked,
	ft_mod_engine_lwpka_gen_mult_mod_locked,
	ft_mod_engine_lwpka_modular_exponentiation_locked,
	ft_mod_engine_lwpka_modular_square_locked,
	ft_mod_engine_pka1_modular_multiplication_locked,
	ft_mod_engine_pka1_modular_addition_locked,
	ft_mod_engine_pka1_modular_subtraction_locked,
	ft_mod_engine_pka1_modular_reduction_locked,
	ft_mod_engine_pka1_modular_division_locked,
	ft_mod_engine_pka1_modular_ilwersion_locked,
	ft_mod_engine_pka1_bit_serial_reduction_locked,
	ft_mod_engine_pka1_bit_serial_double_precision_reduction_locked,
	ft_mod_engine_pka1_gen_mult_locked,
	ft_mod_engine_pka1_c25519_mod_exp_locked,
	ft_mod_engine_pka1_c25519_mod_square_locked,
	ft_mod_engine_pka1_c25519_mod_mult_locked,
	ft_mod_engine_pka1_gen_mult_mod_locked,
	ft_mod_engine_pka1_modular_m521_multiplication_locked,
	ft_mod_engine_pka1_modular_multiplication521_locked,
	ft_mod_last,
};

enum ft_aes_async_e {
	ft_aes_async_none,
	ft_aes_async_first = 110,
	ft_aes_async_engine_aes_locked_async_start,
	ft_aes_async_engine_aes_locked_async_finish,
	ft_aes_async_last,
};

enum ft_aes_poll_e {
	ft_aes_poll_none,
	ft_aes_poll_first = 120,
	ft_aes_poll_engine_aes_locked_async_done,
	ft_aes_poll_last,
};

enum ft_sha_async_e {
	ft_sha_async_none,
	ft_sha_async_first = 130,
	ft_sha_async_engine_sha_locked_async_start,
	ft_sha_async_engine_sha_locked_async_finish,
	ft_sha_async_last,
};

enum ft_sha_poll_e {
	ft_sha_poll_none,
	ft_sha_poll_first = 140,
	ft_sha_poll_engine_sha_locked_async_done,
	ft_sha_poll_last,
};
/*@}*/
#endif /* INCLUDED_CRYPTO_ENGINECALL_H */
