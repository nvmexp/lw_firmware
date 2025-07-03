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

#ifndef INCLUDED_TEGRA_PKA1_EC_GEN_H
#define INCLUDED_TEGRA_PKA1_EC_GEN_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

/* Common include for all PKA implementations for EC support.
 */
#if CCC_WITH_ECC

/* Cert-C arithmetic checks need some arbitrary max length for
 * the ASN.1 encoded EC signature blob.
 *
 * Properly encoded fields generate less than (max) 200 byte signatures
 * but the Wycheproof tests are not properly encoded. They use arbitrary long
 * zero paddings for ASN.1 integer fields and test if the implementation
 * can cope with it.
 *
 * To give some max length in the support to cope with the Cert-C
 * arithmetic overflow checks, use this (arbitrary) max length.
 */
#define MAX_EC_ASN1_SIG_LEN 1024U

#define CCC_EC_POW2_ALIGN(x) \
	(uint32_t)(((x) <= 16U) ? 16U : ((x) <= 32U) ? \
	32U : ((x) <= 64U) ? 64U : ((x) <= 128U) ? 128U : 0U)

struct pka1_ec_lwrve_s;

const struct pka1_ec_lwrve_s *ccc_ec_get_lwrve(te_ec_lwrve_id_t lwrve_id);

#if HAVE_PKA1_SCALAR_MULTIPLIER_CHECK
status_t pka1_ec_scalar_multiplier_valid(se_engine_ec_context_t *econtext,
					 const uint8_t *scalar,
					 uint32_t scalar_len,
					 bool scalar_BE,
					 bool *valid_p);
#endif /* HAVE_PKA1_SCALAR_MULTIPLIER_CHECK */

#if HAVE_ECDSA_SIGN
status_t encode_vector_as_asn1_integer(uint8_t *obuf,
				       const uint8_t *vect, uint32_t vlen,
				       uint32_t *elen_p);
#endif

/* Extract ECDSA fields R and S from an openssl generated ASN.1 encoded
 * binary data blob (EC signature data blob).
 */
status_t ecdsa_get_asn1_ec_signature_coordinate(const uint8_t *asn1_data,
						uint32_t	asn1_bytes,
						uint8_t *cbuf,
						uint32_t clen,
						uint32_t *used_bytes);
/**
 * @brief Parse a ASN.1 encoded integer that fits in 32 bits
 *
 * @param p_a points to the first byte of the integer (after TAG 0x02U)
 * @param val_p returns the 32 bit int value
 * @param len_p returns the number of bytes in the int value (caller must skip)
 * @param maxlen max bytes left in the ASN.1 blob
 *
 * @return NO_ERROR on success, ERR_NOT_VALID otherwise.
 * returned value are valid only on NO_ERROR return.
 */
status_t asn1_get_integer(const uint8_t *p_a, uint32_t *val_p,
			  uint32_t *len_p, uint32_t maxlen);
#endif /* CCC_WITH_ECC */

#endif /* INCLUDED_TEGRA_PKA1_EC_GEN_H */
