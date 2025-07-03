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

#ifndef INCLUDED_TEGRA_PKA1_MOD_H
#define INCLUDED_TEGRA_PKA1_MOD_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

#ifndef HAVE_PKA1_BIT_SERIAL_REDUCTION
#define HAVE_PKA1_BIT_SERIAL_REDUCTION 0
#endif

#ifndef HAVE_PKA1_MODULAR_DIVISION
#define HAVE_PKA1_MODULAR_DIVISION 0
#endif

#define PKA1_MOD_FLAG_NONE		0x0000U
#define PKA1_MOD_FLAG_X_LITTLE_ENDIAN	0x0001U
#define PKA1_MOD_FLAG_Y_LITTLE_ENDIAN	0x0002U
#define PKA1_MOD_FLAG_M_LITTLE_ENDIAN	0x0004U
#define PKA1_MOD_FLAG_R_LITTLE_ENDIAN	0x0008U
#define PKA1_MOD_FLAG_MONT_M_PRIME_LITTLE_ENDIAN	0x0010U
#define PKA1_MOD_FLAG_MONT_R2_LITTLE_ENDIAN		0x0020U

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
	const uint8_t *mont_m_prime;
	const uint8_t *mont_r2;
	uint32_t       size;
	enum pka1_op_mode op_mode;
	uint32_t	mod_flags;
} te_mod_params_t;

/* x*y mod m */
status_t engine_pka1_modular_multiplication_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);

/* x+y mod m */
status_t engine_pka1_modular_addition_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);

/* x-y mod m */
status_t engine_pka1_modular_subtraction_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);

/* x mod m */
status_t engine_pka1_modular_reduction_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);

#if CCC_WITH_MODULAR_DIVISION
/* y/x mod m */
status_t engine_pka1_modular_division_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);
#endif /* CCC_WITH_MODULAR_DIVISION */

/* 1/x mod m */
status_t engine_pka1_modular_ilwersion_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);

#if CCC_WITH_BIT_SERIAL_REDUCTION
/* x mod m */
status_t engine_pka1_bit_serial_reduction_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);
#endif /* CCC_WITH_BIT_SERIAL_REDUCTION */

/* x mod m (double precision x) */
status_t engine_pka1_bit_serial_double_precision_reduction_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);

#if CCC_WITH_ED25519
status_t engine_pka1_c25519_mod_exp_locked(const engine_t *engine,
					   const te_mod_params_t *params);

status_t engine_pka1_c25519_mod_mult_locked(const engine_t *engine,
					    const te_mod_params_t *params);

status_t engine_pka1_c25519_mod_square_locked(const engine_t *engine,
					      const te_mod_params_t *params);
#endif /* CCC_WITH_ED25519 */

#if HAVE_ELLIPTIC_20

#if HAVE_P521
/* Special NIST P521 lwrve operations
 */

/* Modulus must be set to p=2^521-1, generic operands
 * x*y mod p where (0 <= x < p) and (0 <= y < p) and p = 2^251-1
 *
 * Elliptic  MODMULT_521 operation
 */
status_t engine_pka1_modular_m521_multiplication_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);

/* Operands are 521 bit values, generic modulus (usually OBP)!
 * x*y mod m where (0 <= x < m) and (0 <= y < m)
 *
 * Elliptic  M_521_MONTMULT operation (Required pre-computed montgomery values)!
 */
status_t engine_pka1_modular_multiplication521_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);
#endif /* HAVE_P521 */

#if HAVE_GEN_MULTIPLY
/* Non-modular double precision multiply */
status_t engine_pka1_gen_mult_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);

/* Non-modular double precision multiply immediately followed
 * by double precision bit serial reduction
 */
status_t engine_pka1_gen_mult_mod_locked(
	const engine_t  *engine,
	const te_mod_params_t *params);
#endif

#endif /* HAVE_ELLIPTIC_20 */

#endif /* INCLUDED_TEGRA_PKA1_MOD_H */
