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
#ifndef INCLUDED_TEGRA_PKA1_RSA_H
#define INCLUDED_TEGRA_PKA1_RSA_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

#define SE_PKA1_RSA_MAX_MODULUS_SIZE_BITS 4096U
#define SE_PKA1_RSA_MAX_EXPONENT_SIZE_BITS 4096U

struct se_data_params;
struct se_engine_rsa_context;
struct engine_s;

typedef struct {
	const uint8_t *m_prime;
	const uint8_t *r2;
} rsa_montgomery_values_t;

status_t engine_pka1_rsa_modular_exp_locked(
	struct se_data_params *input_params,
	struct se_engine_rsa_context *econtext);

#if HAVE_PKA1_LOAD
/* Should not call these directly from subsys, use via these
 * device switches instead (the switches are not called locked):
 *
 * status_t se_clear_device_rsa_keyslot(se_cdev_id_t device_id, uint32_t keyslot)
 *
 * status_t se_set_device_rsa_keyslot(se_cdev_id_t device_id, uint32_t keyslot,
 *		 uint32_t exponent_size_bytes, uint32_t rsa_size_bytes,
 *		 uint8_t *exponent, uint8_t *modulus,
 *		 bool is_big_endian)
 *
 * status_t se_set_device_rsa_keyslot_montgomery(se_cdev_id_t device_id, uint32_t keyslot,
 *		 uint32_t exponent_size_bytes, uint32_t rsa_size_bytes,
 *		 uint8_t *exponent, uint8_t *modulus,
 *		 bool is_big_endian, uint8_t *m_prime,
 *		 uint8_t *r2)
 */
status_t pka1_clear_rsa_keyslot_locked(const struct engine_s *engine,
				       uint32_t pka_keyslot,
				       uint32_t rsa_size);

status_t pka1_rsa_write_key_locked(const struct engine_s *engine,
				   uint32_t rsa_keyslot,
				   uint32_t rsa_expsize_bytes,
				   uint32_t rsa_size_bytes,
				   const uint8_t *exponent,
				   const uint8_t *modulus,
				   bool exponent_big_endian,
				   bool modulus_big_endian,
				   const rsa_montgomery_values_t *mg);

#define PKA_RSA_WRITE_KEY_LOCKED(eng,kslot,exp_size,rsa_size,exp,mod,e_BE, m_BE, mg) \
	pka1_rsa_write_key_locked(eng,kslot,exp_size,rsa_size,exp,mod,e_BE, m_BE, mg)

#define PKA_CLEAR_RSA_KEYSLOT_LOCKED(eng, kslot, rsa_size)	\
	pka1_clear_rsa_keyslot_locked(eng, kslot, rsa_size)

#endif /* HAVE_PKA1_LOAD */
#endif /* INCLUDED_TEGRA_PKA1_RSA_H */
