/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/*!
 * @file   lw_apm_const.h
 * @brief  This file declares the defines and macros describes the SPDM
 *         capabilities and algorithms that are used for APM implementation.
 */

#ifndef _SPDM_APM_CONST_H_
#define _SPDM_APM_CONST_H_

#include "base.h"
#include "industry_standard/spdm.h"

/* ------------------------- Macros and Defines ----------------------------- */
//
// CTExponent is byte value used to determine when requester can retry message
// due to timeout. This timeout is callwlated as (2^CTExponent) microseconds.
// We expect RM to always block for response - therefore, simply use max value.
//
#define APM_SPDM_CT_EXPONENT           (0xFF)

// APM-TODO CONFCOMP-397: Ensure all required capabilities are added.
#define APM_SPDM_CAPABILITY_FLAGS      (SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CERT_CAP       | \
                                        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_ENCRYPT_CAP    | \
                                        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MAC_CAP        | \
                                        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_KEY_EX_CAP     | \
                                        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MEAS_CAP_SIG   |  \
                                        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MEAS_FRESH_CAP)

#define APM_SPDM_BASE_ASYM_ALGO        (SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P256)
#define APM_SPDM_BASE_HASH_ALGO        (SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_256)
#define APM_SPDM_DHE_GROUP             (SPDM_ALGORITHMS_DHE_NAMED_GROUP_SECP_256_R1)
#define APM_SPDM_REQ_ASYM_ALGO         (SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P256)
#define APM_SPDM_AEAD_SUITE            (SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_128_CTR_HMAC_SHA_256)
#define APM_SPDM_MEASUREMENT_SPEC      (SPDM_MEASUREMENT_BLOCK_HEADER_SPECIFICATION_DMTF)
#define APM_SPDM_MEASUREMENT_HASH_ALGO (SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_256)
#define APM_SPDM_KEY_SCHEDULE          (SPDM_ALGORITHMS_KEY_SCHEDULE_HMAC_HASH)

#define APM_SPDM_CERT_SLOT_COUNT            (2)
#define APM_SPDM_CERT_SLOT_ID_KX_CERT       (0)
#define APM_SPDM_CERT_SLOT_ID_AK_CERT       (1)

#endif // _SPDM_APM_CONST_H_
