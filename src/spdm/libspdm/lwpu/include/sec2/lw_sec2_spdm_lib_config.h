/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

//
// LWE (tandrewprinz): Including LWPU copyright, as file was forked from
// libspdm (include/spdm_lib_config.h), but heavily modified by LWPU.
//
// See spdm_lib_config.h for default values.
//

#ifndef _LW_SEC2_SPDM_LIB_CONFIG_
#define _LW_SEC2_SPDM_LIB_CONFIG_

/*!
 * @file   lw_sec2_spdm_lib_config.h
 * @brief  Fork of spdm_lib_config.h from libspdm. Provides definitions
 *         for various libspdm configuration values. See spdm_lib_config.h
 *         for default values.
 */

/* ------------------------- Application Includes --------------------------- */
#include "lib_intfcshahw.h"
#include "apm_rts.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define DEFAULT_CONTEXT_LENGTH                  MAX_HASH_SIZE
#define DEFAULT_SELWRE_MCTP_PADDING_SIZE        1

#define MAX_SPDM_PSK_HINT_LENGTH                16

#define MAX_SPDM_MEASUREMENT_BLOCK_COUNT        APM_MSR_NUM
#define MAX_SPDM_MEASUREMENT_RECORD_SIZE        (APM_MSR_NUM * (APM_HASH_SIZE_BYTE + sizeof(spdm_measurement_block_dmtf_t)))

#define MAX_SPDM_SESSION_COUNT                  1

#define MAX_SPDM_CERT_CHAIN_SIZE                0x400
#define MAX_SPDM_CERT_CHAIN_BLOCK_LEN           0x400

#define MAX_SPDM_MESSAGE_BUFFER_SIZE            0x700
#define MAX_SPDM_MESSAGE_SMALL_BUFFER_SIZE      0x100


//
// NOTE: The following configuration may not support CHALLENGE, as max
// buffer size is not large enough to hold message buffers A, B, and C.
// Fine for now, as we do not expect to support it.
//
#define MAX_SPDM_MESSAGE_A_BUFFER_SIZE          0x100
#define MAX_SPDM_MESSAGE_B_BUFFER_SIZE          0x700
#define MAX_SPDM_MESSAGE_C_BUFFER_SIZE          0x100
#define MAX_SPDM_MESSAGE_M_BUFFER_SIZE          0x700
#define MAX_SPDM_MESSAGE_K_BUFFER_SIZE          0x200
#define MAX_SPDM_MESSAGE_F_BUFFER_SIZE          0x200

#define MAX_SPDM_REQUEST_RETRY_TIMES            1
#define MAX_SPDM_SESSION_STATE_CALLBACK_NUM     1
#define MAX_SPDM_CONNECTION_STATE_CALLBACK_NUM  1

//
// Crypto Configuation
// In each category, at least one should be selected.
//
#define OPENSPDM_RSA_SSA_SUPPORT                0
#define OPENSPDM_RSA_PSS_SUPPORT                0
#define OPENSPDM_ECDSA_SUPPORT                  1

#define OPENSPDM_FFDHE_SUPPORT                  0
#define OPENSPDM_ECDHE_SUPPORT                  1

#define OPENSPDM_AEAD_AES_CTR_HMAC_SHA          1
#define OPENSPDM_AEAD_GCM_SUPPORT               0
#define OPENSPDM_AEAD_CHACHA20_POLY1305_SUPPORT 0

#define OPENSPDM_SHA256_SUPPORT                 1
#define OPENSPDM_SHA384_SUPPORT                 0
#define OPENSPDM_SHA512_SUPPORT                 0

#endif // _LW_SEC2_SPDM_LIB_CONFIG_
