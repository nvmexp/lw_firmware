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
#ifndef INCLUDED_TEGRA_LWPKA_EC_H
#define INCLUDED_TEGRA_LWPKA_EC_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

struct se_data_params;
struct se_engine_ec_context;

status_t lwpka_ec_init(struct se_engine_ec_context *econtext,
		       enum lwpka_engine_ops eop);

status_t lwpka_ec_engine_op(struct se_data_params *input_params,
			    struct se_engine_ec_context *econtext,
			    enum lwpka_engine_ops eop);

void lwpka_data_ec_release(struct se_engine_ec_context *econtext);

/* LWPKA engine support macros for EC
 */
#define PKA_DATA_EC_RELEASE(ectx) lwpka_data_ec_release(ectx)

/***************/

status_t engine_lwpka_ec_point_mult_locked(struct se_data_params *input_params,
					   struct se_engine_ec_context *econtext);

status_t engine_lwpka_ec_point_verify_locked(struct se_data_params *input_params,
					     struct se_engine_ec_context *econtext);

status_t engine_lwpka_ec_shamir_trick_locked(struct se_data_params *input_params,
					     struct se_engine_ec_context *econtext);

#if CCC_WITH_MONTGOMERY_ECDH
status_t engine_lwpka_montgomery_point_mult_locked(struct se_data_params *input_params,
						   struct se_engine_ec_context *econtext);
#endif /* CCC_WITH_MONTGOMERY_ECDH */

#if CCC_WITH_EDWARDS
status_t engine_lwpka_edwards_point_mult_locked(struct se_data_params *input_params,
						struct se_engine_ec_context *econtext);

status_t engine_lwpka_edwards_point_verify_locked(struct se_data_params *input_params,
						  struct se_engine_ec_context *econtext);

status_t engine_lwpka_edwards_shamir_trick_locked(struct se_data_params *input_params,
						  struct se_engine_ec_context *econtext);
#endif /* CCC_WITH_EDWARDS */

#endif /* INCLUDED_TEGRA_LWPKA_EC_H */
