/*
 * Copyright (c) 2016-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */
#ifndef INCLUDED_CRYPTO_NIST_KDF_COUNTER_H
#define INCLUDED_CRYPTO_NIST_KDF_COUNTER_H

#define NIST_KDF_COUNTER_MODE_CTR_VALUE 1U /* Initial counter mode value in spec */

#define KDF_VALUE_ENCODING_LENGTH 4U /* Encode int values as 4 byte octet strings
				      * in NIST 800-108 encoding (consensus in LW).
				      */

#define KDF_COUNTER_MAX_VALUE 2U /* Lwrrently no key derivations with larger counters
				  */

#ifndef MAX_KDF_ENCODING_LENGTH
#define MAX_KDF_ENCODING_LENGTH 512U /* Arbitrary byte size limit for NIST 800-108
				      * encoding result for <context,label>
				      */
#endif

#if CCC_WITH_NIST_KDF_COUNTER_ENCODER && HAVE_EXPORT_NIST_COUNTER_MODE_ENCODER
/* Export the NIST 800-108 encoder if required by client.
 */
status_t kdf_nist_encode_counter_mode(uint32_t counter,
				      uint8_t *buf,
				      uint32_t buf_len,
				      uint32_t key_bits,
				      const uint8_t *kdf_label,
				      uint32_t kdf_label_len,
				      const uint8_t *kdf_context,
				      uint32_t kdf_context_len);
#endif /* CCC_WITH_NIST_KDF_COUNTER_ENCODER && HAVE_EXPORT_NIST_COUNTER_MODE_ENCODER */

#endif /* INCLUDED_CRYPTO_NIST_KDF_COUNTER_H */
