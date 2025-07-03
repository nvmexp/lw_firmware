/*
 * Copyright (c) 2018-2020, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */
/**
 * @file   crypto_ops_gcm.h
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2019
 *
 * Definitions for common crypto AES-GCM mode.
 * These are used by CCC internally.
 */
#ifndef INCLUDED_CRYPTO_OPS_GCM_H
#define INCLUDED_CRYPTO_OPS_GCM_H

#ifndef CCC_SOC_FAMILY_DEFAULT
#include <crypto_system_config.h>
#endif

#define AES_GCM_J0_COUNTER_VALUE 0x00000001U

#ifndef AES_GCM_MIN_TAG_LENGTH
#define AES_GCM_MIN_TAG_LENGTH  9U
#endif

#ifndef CCC_AES_GCM_DECIPHER_COMPARE_TAG
/* If this is defined 1 AES-GCM deciphering requires caller
 * to input the correct TAG value in operation INIT arguments.
 *
 * In DOFINAL the provided tag value is compared with the provided tag
 * and the operation will FAIL if the provided tag does not match
 * with the callwlated tag (ERR_NOT_VALID).
 */
#define CCC_AES_GCM_DECIPHER_COMPARE_TAG 1
#endif

#if CCC_WITH_AES_GCM

status_t cinit_gcm(te_crypto_domain_t domain,
		   struct se_aes_context *aes_context);

status_t ae_gcm_finalize_tag(struct se_aes_context *aes_ctx);

/* Callback handler (with USER_MODE) */
status_t se_aes_gcm_process_input(
	struct se_data_params *input_params,
	struct se_aes_context *aes_context);

#endif /* CCC_WITH_AES_GCM */
#endif /* INCLUDED_CRYPTO_OPS_GCM_H */
