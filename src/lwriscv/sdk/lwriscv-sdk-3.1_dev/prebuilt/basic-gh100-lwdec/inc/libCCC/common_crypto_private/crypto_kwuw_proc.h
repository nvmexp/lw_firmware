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
#ifndef INCLUDED_CRYPTO_KWUW_PROC_H
#define INCLUDED_CRYPTO_KWUW_PROC_H

#include <crypto_ops.h>

#define KW_GCM_IV_FIXED_LENGTH	12U

#define KAC_KEY_WRAP_BLOB_LEN		   64U	/* Length of KW generated blob */
#define KAC_KEY_WRAP_DST_WKEY_MAX_BYTE_LEN 32U	/* Wrapped key is zero padded to 256
						 * bits (max 32 bytes)
						 */
#endif /* INCLUDED_CRYPTO_KWUW_PROC_H */
