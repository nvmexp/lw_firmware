/*
 * Copyright (c) 2019-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */
#ifndef INCLUDED_TEGRA_LWPKA_RSA_H
#define INCLUDED_TEGRA_LWPKA_RSA_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

#define SE_LWPKA_RSA_MAX_MODULUS_SIZE_BITS  4096U
#define SE_LWPKA_RSA_MAX_EXPONENT_SIZE_BITS 4096U

struct se_data_params;
struct se_engine_rsa_context;
struct engine_s;

status_t engine_lwpka_rsa_modular_exp_locked(
	struct se_data_params *input_params,
	struct se_engine_rsa_context *econtext);

#if HAVE_LWPKA_LOAD
/* Should not call these lwpka functions directly from subsys, use via these
 * device switches instead (the switches are not called locked):
 *
 * status_t se_clear_device_rsa_keyslot(se_cdev_id_t device_id, uint32_t keyslot)
 *
 * status_t se_set_device_rsa_keyslot(se_cdev_id_t device_id, uint32_t keyslot,
 *		 uint32_t exponent_size_bytes, uint32_t rsa_size_bytes,
 *		 uint8_t *exponent, uint8_t *modulus,
 *		 bool is_big_endian)
 */
status_t lwpka_clear_rsa_keyslot_locked(const struct engine_s *engine,
					uint32_t pka_keyslot,
					uint32_t rsa_size);
#endif /* HAVE_LWPKA_LOAD */

status_t lwpka_rsa_write_key_locked(const struct engine_s *engine,
				    uint32_t rsa_keyslot,
				    uint32_t rsa_expsize_bytes,
				    uint32_t rsa_size_bytes,
				    const uint8_t *exponent,
				    const uint8_t *modulus,
				    bool exponent_big_endian,
				    bool modulus_big_endian);

/* LWPKA does not use Montgomery constants, no need to pass that to callee
 */
#define PKA_RSA_WRITE_KEY_LOCKED(eng,kslot,exp_size,rsa_size,exp,mod,e_BE, m_BE, mg) \
	lwpka_rsa_write_key_locked(eng,kslot,exp_size,rsa_size,exp,mod,e_BE, m_BE)

#define PKA_CLEAR_RSA_KEYSLOT_LOCKED(eng, kslot, rsa_size)	\
	lwpka_clear_rsa_keyslot_locked(eng, kslot, rsa_size)

/* No Montgomery values passed or used in LWPKA code, passing NULL references
 * to maintain API compatibility.
 */
typedef void rsa_montgomery_values_t;

#endif /* INCLUDED_TEGRA_LWPKA_RSA_H */
