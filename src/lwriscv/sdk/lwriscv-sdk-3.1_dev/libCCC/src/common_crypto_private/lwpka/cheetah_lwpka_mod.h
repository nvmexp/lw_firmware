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

#ifndef INCLUDED_TEGRA_LWPKA_MOD_H
#define INCLUDED_TEGRA_LWPKA_MOD_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

#define PKA1_MOD_FLAG_NONE			0x0000U
#define PKA1_MOD_FLAG_X_LITTLE_ENDIAN		0x0001U
#define PKA1_MOD_FLAG_Y_LITTLE_ENDIAN		0x0002U
#define PKA1_MOD_FLAG_M_LITTLE_ENDIAN		0x0004U
#define PKA1_MOD_FLAG_R_LITTLE_ENDIAN		0x0008U
#define PKA1_MOD_FLAG_MONT_M_PRIME_LITTLE_ENDIAN 0x0010U
#define PKA1_MOD_FLAG_MONT_R2_LITTLE_ENDIAN	0x0020U

/* common size for x,y,m,r etc.
 *
 * When double precision bit serial reduction is used x is 2*size
 */
typedef struct te_mod_params_s {
	struct context_mem_s *cmem;
	const uint8_t *x;
	const uint8_t *y;
	const uint8_t *m;
	uint8_t       *r;
	uint32_t       size;
	enum pka1_op_mode op_mode;
	uint32_t	mod_flags;
} te_mod_params_t;

/* x*y mod m */
status_t engine_lwpka_modular_multiplication_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);

/* x+y mod m */
status_t engine_lwpka_modular_addition_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);

/* x-y mod m */
status_t engine_lwpka_modular_subtraction_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);

/* x ^ y mod m */
status_t engine_lwpka_modular_exponentiation_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);

/* x mod m (single precision) */
status_t engine_lwpka_modular_reduction_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);

/* x mod m (double precision) */
status_t engine_lwpka_modular_dp_reduction_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);

status_t engine_lwpka_modular_square_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);

#if CCC_WITH_MODULAR_DIVISION
/* y/x mod m */
status_t engine_lwpka_modular_division_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);
#endif /* CCC_WITH_MODULAR_DIVISION */

/* 1/x mod m */
status_t engine_lwpka_modular_ilwersion_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);

#if HAVE_GEN_MULTIPLY
/* Non-modular double precision multiply */
status_t engine_lwpka_gen_mult_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);

/* Non-modular double precision multiply immediately followed
 * by double precision bit serial reduction
 */
status_t engine_lwpka_gen_mult_mod_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);
#endif /* HAVE_GEN_MULTIPLY */

#endif /* INCLUDED_TEGRA_LWPKA_MOD_H */
