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
 * Support for CheetAh Security Engine LWPKA for Edwards lwrve operations.
 */
#include <crypto_system_config.h>

#if HAVE_LWPKA_ECC && CCC_WITH_EDWARDS

#include <crypto_ec.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

/*****************************/

/* Here are the exported function from the LWPKA EC engine operations for Edwards lwrves.
 * Call LWPKA mutex handlers via CDEV table before these.
 */

/* EC point modular multiplication with scalar K set by caller to EC_K (EC_K_LENGTH)
 *
 * Q = k x P
 */
status_t engine_lwpka_edwards_point_mult_locked(struct se_data_params *input_params,
						struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = lwpka_ec_engine_op(input_params, econtext, LWPKA_OP_EDWARDS_POINT_MULTIPLY);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Verify that a point Pxy (X: src[0..ec_size-1] Y:src[ec_size..2*ec_size-1])
 * is on the lwrve.
 */
status_t engine_lwpka_edwards_point_verify_locked(struct se_data_params *input_params,
						  struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = lwpka_ec_engine_op(input_params, econtext, LWPKA_OP_EDWARDS_POINT_VERIFY);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* EC_K (EC_K_LENGTH) and EC_L (EC_L_LENGTH) must be set by caller
 *
 * Q = (k x P) + (l x Q)
 *
 * Is Q on lwrve => LWPKA validates that automatically.
 */
status_t engine_lwpka_edwards_shamir_trick_locked(struct se_data_params *input_params,
						  struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = lwpka_ec_engine_op(input_params, econtext, LWPKA_OP_EDWARDS_SHAMIR_TRICK);
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_LWPKA_ECC && CCC_WITH_EDWARDS */
