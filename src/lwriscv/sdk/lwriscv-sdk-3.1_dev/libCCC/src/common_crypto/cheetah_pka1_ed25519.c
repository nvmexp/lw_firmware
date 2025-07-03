/*
 * Copyright (c) 2017-2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */
/*
 * Support for CheetAh Security Engine Elliptic crypto point operations.
 */
#include <crypto_system_config.h>

#if HAVE_PKA1_ECC && HAVE_PKA1_ED25519

#include <crypto_ec.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

/*****************************/

/* Here are the exported function from the PKA1 EC engine operations for ED25519 lwrve.
 * Everything in this file is called with the PKA mutex locked via CDEV table.
 */

/* EC point modular multiplication with scalar K set by caller to EC_K (EC_K_LENGTH)
 *
 * Q = k x P
 */
status_t engine_pka1_ed25519_point_mult_locked(struct se_data_params *input_params,
					       struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = pka1_ec_engine_op(input_params, econtext, PKA1_OP_ED25519_POINT_MULTIPLY);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_PKA1_ED25519_POINT_ADD
/* EC point modular addition: Q = P + Q */
status_t engine_pka1_ed25519_point_add_locked(struct se_data_params *input_params,
					      struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = pka1_ec_engine_op(input_params, econtext, PKA1_OP_ED25519_POINT_ADD);
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_PKA1_ED25519_POINT_ADD */

#if HAVE_PKA1_ED25519_POINT_DOUBLE
/* EC point double: Q = P + P */
status_t engine_pka1_ed25519_point_double_locked(struct se_data_params *input_params,
						 struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = pka1_ec_engine_op(input_params, econtext, PKA1_OP_ED25519_POINT_DOUBLE);
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_PKA1_ED25519_POINT_DOUBLE */

/* Verify that a point Pxy (X: src[0..ec_size-1] Y:src[ec_size..2*ec_size-1])
 * is on the lwrve.
 *
 * Is P on lwrve?
 */
status_t engine_pka1_ed25519_point_verify_locked(struct se_data_params *input_params,
						 struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = pka1_ec_engine_op(input_params, econtext, PKA1_OP_ED25519_POINT_VERIFY);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* EC_K (EC_K_LENGTH) and EC_L (EC_L_LENGTH) must be set by caller
 *
 * Q = (k x P) + (l x Q)
 */
status_t engine_pka1_ed25519_shamir_trick_locked(struct se_data_params *input_params,
						 struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = pka1_ec_engine_op(input_params, econtext, PKA1_OP_ED25519_SHAMIR_TRICK);
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_PKA1_ECC && HAVE_PKA1_ED25519 */
