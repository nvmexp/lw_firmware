/*
 * Copyright (c) 2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/* PKA1 engine keyslot reader header */

#ifndef INCLUDED_TEGRA_PKA1_KS_READ_H
#define INCLUDED_TEGRA_PKA1_KS_READ_H

#if HAVE_PKA1_RSA && HAVE_READ_PKA1_RSA_KEYSLOT_PUBKEY

#ifndef RSA_PUBLIC_EXPONENT_BYTE_SIZE
#define RSA_PUBLIC_EXPONENT_BYTE_SIZE 4U
#endif

#ifndef MAX_RSA_BYTE_SIZE
#define MAX_RSA_BYTE_SIZE (4096U / 8U)
#endif

/* Object to hold RSA public key from keyslot.
 */
struct ks_rsa_data {
	uint8_t rsa_pub_exponent[RSA_PUBLIC_EXPONENT_BYTE_SIZE];
	uint8_t rsa_modulus[MAX_RSA_BYTE_SIZE];
	uint8_t rsa_r2_sqr[MAX_RSA_BYTE_SIZE];
	uint8_t rsa_m_prime[MAX_RSA_BYTE_SIZE];
};

status_t pka1_read_rsa_keyslot(engine_id_t eid,
			       uint32_t rsa_keyslot,
			       uint32_t rsa_bit_len,
			       bool data_big_endian,
			       struct ks_rsa_data *ks_data);
#endif /* HAVE_PKA1_RSA */

#if HAVE_PKA1_ECC

#if HAVE_READ_PKA1_EC_KEYSLOT_PUBKEY
/** Reads EC public key point <X,Y> from given keyslot to PUBKEY.
 *
 * point_flags CCC_EC_POINT_FLAG_LITTLE_ENDIAN can be set if point
 * should be stored in pubkey parameter in little endian byte order.
 *
 * For ED25519 lwrve the public key in the keyslot could be compressed
 * and in such case the CCC_EC_POINT_FLAG_COMPRESSED_ED25519 must be
 * set to point_flags.  (for a compressed point there is no Y
 * co-ordinate. The Y co-ordinate is encoded into the 32 byte X
 * co-ordinate and CCC will perform the decompression on signature
 * handling). The compressed point ofr this lwrve is little endian value.
 */
status_t pka1_read_ec_point_keyslot(engine_id_t eid,
				    uint32_t ec_keyslot,
				    te_ec_lwrve_id_t lwrve_id,
				    uint32_t point_flags,
				    te_ec_point_t *pubkey);
#endif

#if HAVE_WRITE_PKA1_EC_KEYSLOT_PUBKEY
/** Writes EC public key point <X,Y> to given ec_keyslot from PUBKEY.
 *
 * The point flags are in the pubkey object.
 *
 * Writes KS <X,Y> co-ordinates as indicated by lwrve_id.
 * The engine object can be selected by ENGINE_PKA1_PKA engine id
 * prior to the call.
 *
 * For ED25519 lwrve the public key can be compressed or uncompressed
 * as per pubkey flags. A compressed 32 byte pubkey is written to
 * keyslot field <X>. Field <Y> is not modified in this case.
 *
 * Compressed ED25519 public keys are expected to be in LITTLE ENDIAN
 * byte order even without the CCC_EC_POINT_FLAG_LITTLE_ENDIAN flag.
 */
status_t pka1_write_ec_point_keyslot(
	engine_id_t eid,
	uint32_t ec_keyslot,
	te_ec_lwrve_id_t lwrve_id,
	const te_ec_point_t *pubkey);
#endif

#endif /* HAVE_PKA1_ECC */

#endif /* INCLUDED_TEGRA_PKA1_KS_READ_H */
