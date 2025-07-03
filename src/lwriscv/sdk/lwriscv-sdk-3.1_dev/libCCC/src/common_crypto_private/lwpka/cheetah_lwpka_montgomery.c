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
 * Support for CheetAh Security Engine elliptic crypto point multiplication
 * for montgomery lwrves.
 */
#include <crypto_system_config.h>

#if HAVE_LWPKA_ECC && CCC_WITH_X25519

#include <crypto_ec.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

/*****************************/

/* Here are the exported function from the PKA1 engine Lwrve25519 operations.
 * Everything in this file is called with the PKA mutex locked via CDEV table.
 */

/* X25519/X448 point modular multiplication with scalar K set
 * by caller to EC_K (EC_K_LENGTH) with Lwrve25519/Lwrve448
 *
 * Q = k x P
 */
status_t engine_lwpka_montgomery_point_mult_locked(struct se_data_params *input_params,
						   struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = lwpka_ec_engine_op(input_params, econtext,
				 LWPKA_OP_MONTGOMERY_POINT_MULTIPLY);
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_LWPKA_ECC && CCC_WITH_X25519 */
