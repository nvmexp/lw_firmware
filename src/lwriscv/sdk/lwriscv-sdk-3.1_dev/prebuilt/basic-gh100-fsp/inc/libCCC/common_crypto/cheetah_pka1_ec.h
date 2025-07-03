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
#ifndef INCLUDED_TEGRA_PKA1_EC_H
#define INCLUDED_TEGRA_PKA1_EC_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

struct se_data_params;
struct se_engine_ec_context;

status_t pka1_ec_engine_op(struct se_data_params *input_params,
			   struct se_engine_ec_context *econtext,
			   enum pka1_engine_ops pka1_op);

void pka1_data_ec_release(struct se_engine_ec_context *econtext);

/* PKA1 engine support macros for EC
 */
#define PKA_DATA_EC_RELEASE(ectx) pka1_data_ec_release(ectx)

/**************/

status_t engine_pka1_ec_point_mult_locked(struct se_data_params *input_params,
					  struct se_engine_ec_context *econtext);

#if HAVE_PKA1_ECC_POINT_ADD
status_t engine_pka1_ec_point_add_locked(struct se_data_params *input_params,
					 struct se_engine_ec_context *econtext);
#endif /* HAVE_PKA1_ECC_POINT_ADD */

#if HAVE_PKA1_ECC_POINT_DOUBLE
status_t engine_pka1_ec_point_double_locked(struct se_data_params *input_params,
					    struct se_engine_ec_context *econtext);
#endif /* HAVE_PKA1_ECC_POINT_DOUBLE */

status_t engine_pka1_ec_point_verify_locked(struct se_data_params *input_params,
					    struct se_engine_ec_context *econtext);

status_t engine_pka1_ec_shamir_trick_locked(struct se_data_params *input_params,
					    struct se_engine_ec_context *econtext);

#if HAVE_PKA1_X25519
status_t engine_pka1_c25519_point_mult_locked(struct se_data_params *input_params,
					      struct se_engine_ec_context *econtext);
#endif /* HAVE_PKA1_X25519 */

#if HAVE_PKA1_ED25519
status_t engine_pka1_ed25519_point_mult_locked(struct se_data_params *input_params,
					       struct se_engine_ec_context *econtext);

#if HAVE_PKA1_ED25519_POINT_DOUBLE
status_t engine_pka1_ed25519_point_double_locked(struct se_data_params *input_params,
						 struct se_engine_ec_context *econtext);
#endif /* HAVE_PKA1_ED25519_POINT_DOUBLE */

#if HAVE_PKA1_ED25519_POINT_ADD
status_t engine_pka1_ed25519_point_add_locked(struct se_data_params *input_params,
					      struct se_engine_ec_context *econtext);
#endif /* HAVE_PKA1_ED25519_POINT_ADD */

status_t engine_pka1_ed25519_point_verify_locked(struct se_data_params *input_params,
						 struct se_engine_ec_context *econtext);

status_t engine_pka1_ed25519_shamir_trick_locked(struct se_data_params *input_params,
						 struct se_engine_ec_context *econtext);
#endif

#if HAVE_P521 && HAVE_ELLIPTIC_20
status_t engine_pka1_ec_p521_point_mult_locked(struct se_data_params *input_params,
					       struct se_engine_ec_context *econtext);

#if HAVE_PKA1_P521_POINT_DOUBLE
status_t engine_pka1_ec_p521_point_double_locked(struct se_data_params *input_params,
						 struct se_engine_ec_context *econtext);
#endif /* HAVE_PKA1_P521_POINT_DOUBLE */

#if HAVE_PKA1_P521_POINT_ADD
status_t engine_pka1_ec_p521_point_add_locked(struct se_data_params *input_params,
					      struct se_engine_ec_context *econtext);
#endif /* HAVE_PKA1_P521_POINT_ADD */

status_t engine_pka1_ec_p521_point_verify_locked(struct se_data_params *input_params,
						 struct se_engine_ec_context *econtext);

status_t engine_pka1_ec_p521_shamir_trick_locked(struct se_data_params *input_params,
						 struct se_engine_ec_context *econtext);
#endif
#endif /* INCLUDED_TEGRA_PKA1_EC_H */
