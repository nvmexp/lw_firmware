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
// LWE (tandrewprinz): Including LWPU copyright, as file has been heavily
// modified by LWPU, in order to support multiple versions of the library.
//

#ifndef __SPDM_LIB_CONFIG_H__
#define __SPDM_LIB_CONFIG_H__

//
// LWE (tandrewprinz): Since the configuration values vary widely by
// implementation, we leave it up to the implementation to provide their own
// desired values via the header file includes below.
//
// For any future integrations of this file, ensure the relevant changes
// are also integrated to the files below.
//
#if defined(SEC2_RTOS)
#include "lw_sec2_spdm_lib_config.h"
#elif defined(GSP_CC)
#include "lw_gsp_spdm_lib_config.h"
#else
//
// LWE (tandrewprinz): Provide a copy of the default define values, as well
// as any LW-lwstomizations, as reference for any future implementations.
// We assert failure, as we don't want to use defaults on inclusion miss above.
//
#include "lwctassert.h"
ct_assert(0);

#define DEFAULT_CONTEXT_LENGTH MAX_HASH_SIZE
#define DEFAULT_SELWRE_MCTP_PADDING_SIZE 1

#define MAX_SPDM_PSK_HINT_LENGTH 16

#define MAX_SPDM_MEASUREMENT_BLOCK_COUNT 8
#define MAX_SPDM_SESSION_COUNT 4
#define MAX_SPDM_CERT_CHAIN_SIZE 0x1000
#define MAX_SPDM_MEASUREMENT_RECORD_SIZE 0x1000
#define MAX_SPDM_CERT_CHAIN_BLOCK_LEN 1024

#define MAX_SPDM_MESSAGE_BUFFER_SIZE 0x1200
#define MAX_SPDM_MESSAGE_SMALL_BUFFER_SIZE 0x100

// LWE (tandrewprinz) LW_REFACTOR_TRANSCRIPT: Use specific sizes for transcript.
#define MAX_SPDM_MESSAGE_A_BUFFER_SIZE 0x100
#define MAX_SPDM_MESSAGE_B_BUFFER_SIZE 0x500
#define MAX_SPDM_MESSAGE_C_BUFFER_SIZE 0x100
#define MAX_SPDM_MESSAGE_M_BUFFER_SIZE 0x500
#define MAX_SPDM_MESSAGE_K_BUFFER_SIZE 0x200
#define MAX_SPDM_MESSAGE_F_BUFFER_SIZE 0x200

#define MAX_SPDM_REQUEST_RETRY_TIMES 3
#define MAX_SPDM_SESSION_STATE_CALLBACK_NUM 4
#define MAX_SPDM_CONNECTION_STATE_CALLBACK_NUM 4

//
// Crypto Configuation
// In each category, at least one should be selected.
//
#define OPENSPDM_RSA_SSA_SUPPORT 1
#define OPENSPDM_RSA_PSS_SUPPORT 1
#define OPENSPDM_ECDSA_SUPPORT 1

#define OPENSPDM_FFDHE_SUPPORT 1
#define OPENSPDM_ECDHE_SUPPORT 1

// LWE (tandrewprinz) LW_LWSTOM_ALG: Define macro for custom AEAD. 
#define OPENSPDM_AEAD_AES_CTR_HMAC_SHA 1
#define OPENSPDM_AEAD_GCM_SUPPORT 1
#define OPENSPDM_AEAD_CHACHA20_POLY1305_SUPPORT 1

#define OPENSPDM_SHA256_SUPPORT 1
#define OPENSPDM_SHA384_SUPPORT 1
#define OPENSPDM_SHA512_SUPPORT 1

#endif // Default library values.
#endif // __SPDM_LIB_CONFIG_H__
