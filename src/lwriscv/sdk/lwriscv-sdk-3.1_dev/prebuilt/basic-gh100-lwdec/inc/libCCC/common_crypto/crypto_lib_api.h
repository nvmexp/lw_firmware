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

/**
 * @file   crypto_lib_api.h
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2016
 *
 * Definitions for CCC library call functions, e.g. for
 * keyslot management.
 *
 * These are used by CCC internally but also required
 * when CCC is linked to the subsystem as a library.
 */
#ifndef INCLUDED_CRYPTO_LIB_API_H
#define INCLUDED_CRYPTO_LIB_API_H

#ifndef CCC_SOC_FAMILY_DEFAULT
/* Some subsystems do not include the CCC config file before including
 * other CCC headers; normally crypto_ops.h. So include the config
 * here unless the soc family is defined.
 */
#include <crypto_system_config.h>
#endif

#if CCC_WITH_RSA
/**
 * @brief CCC keyslot manipulation: Clear RSA keyslot in PKA0 or PKA1 device
 *
 * NOTE: Clearing a keyslot now means writing zeros to it.
 * Maybe better would be to write a random value from DRNG.
 *
 * @param device_id identify device with SE_CDEV_* (enum se_cdev_id_e value)
 * @param keyslot index of keyslot to clear [0..4] inclusive (PKA0 or PKA1 keyslot)
 * @return NO_ERROR on success, error code otherwise.
 */
status_t se_clear_device_rsa_keyslot(se_cdev_id_t device_id, uint32_t keyslot);

/**
 * @brief CCC keyslot manipulation: Set RSA keyslot for PKA0 or PKA1 device
 *
 * @param device_id identify device with SE_CDEV_* (enum se_cdev_id_e value)
 * @param keyslot index of keyslot to set [0..4] inclusive (PKA0 or PKA1 keyslot)
 * @param exponent_size_bytes exponent byte size (4 for public exponent, rsa size otherwise)
 * @param rsa_size_bytes modulus byte size
 * @param exponent exponent buffer
 * @param modulus modulus buffer
 * @param is_big_endian exponent and modulus are big endian
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t se_set_device_rsa_keyslot(se_cdev_id_t device_id, uint32_t keyslot,
				   uint32_t exponent_size_bytes, uint32_t rsa_size_bytes,
				   const uint8_t *exponent, const uint8_t *modulus,
				   bool is_big_endian);

#if CCC_WITH_PKA_KEYLOAD
/**
 * @brief CCC keyslot manipulation: Set RSA keyslot for device
 *
 * As the se_set_device_rsa_keyslot(), but if loading RSA keys to
 * PKA1 RSA keyslots this version uses OUT-OF-BAND
 * Montgomery values instead of callwlating them
 * using PKA1.
 *
 * Note: Every RSA keypair has different Montgomery values, because
 * they depend on the values of the modulus (for EC lwrves they are
 * constants). This call exists only for speed optimization
 * purposes in the subsystems that can directly call this function,
 * it is not (at least lwrrenlty) exported via the CCC crypto API.
 *
 * Function is only exported if PKA1 key loading is supported.
 *
 * FIXME: (cast alignment)
 * The array values are accessed as words via the pointer =>
 * should CHANGE THE API to require (uint32_t *) pointers
 * instead! Callers should pass word aligned addresses
 * anyway to this, even though most CPUs can deal
 * with unaligned pointer accesses.
 *
 * @param device_id identify device with SE_CDEV_* (enum se_cdev_id_e value)
 * @param keyslot index of keyslot to set [0..4] inclusive (PKA0 or PKA1 keyslot)
 * @param exponent_size_bytes exponent byte size (4 for public exponent, rsa size otherwise)
 * @param rsa_size_bytes modulus byte size
 * @param exponent exponent buffer
 * @param modulus modulus buffer
 * @param is_big_endian exponent and modulus are big endian
 * @param m_prime M_PRIME montgomery value
 * @param r2 R_SQR montgomery value
 * @return NO_ERROR on success, error code otherwise.
 */
status_t se_set_device_rsa_keyslot_montgomery(se_cdev_id_t device_id, uint32_t keyslot,
					      uint32_t exponent_size_bytes, uint32_t rsa_size_bytes,
					      const uint8_t *exponent, const uint8_t *modulus,
					      bool is_big_endian, const uint8_t *m_prime,
					      const uint8_t *r2);
#endif /* CCC_WITH_PKA_KEYLOAD */
#endif /* CCC_WITH_RSA */

#if HAVE_SE_AES
#if HAVE_CCC_LIBRARY_MODE_CALLS

/* Clear/set AES keyslot in SE device */

/**
 * @brief Clear AES keyslot
 * Clears 256 bit key from the keyslot.
 *
 * @param device_id identify device with SE_CDEV_* (enum se_cdev_id_e value)
 * @param keyslot index of keyslot to set [0..15] inclusive (SE AES keyslot)
 * @return NO_ERROR on success, error code otherwise.
 */
status_t se_clear_device_aes_keyslot(se_cdev_id_t device_id, uint32_t keyslot);

/**
 * @brief Set AES keyslot
 *
 * @param device_id identify device with SE_CDEV_* (enum se_cdev_id_e value)
 * @param keyslot index of keyslot to set [0..15] inclusive (SE AES keyslot)
 * @param keydata Key material bytes
 * @param keysize_bits bit length of keydata bytes
 * @return NO_ERROR on success, error code otherwise.
 */
status_t se_set_device_aes_keyslot(se_cdev_id_t device_id, uint32_t keyslot,
				   const uint8_t *keydata, uint32_t keysize_bits);

/**
 * @brief Set AES keyslot with IV
 *
 * @param device_id identify device with SE_CDEV_* (enum se_cdev_id_e value)
 * @param keyslot index of keyslot to set [0..15] inclusive (SE AES keyslot)
 * @param keydata Key material bytes
 * @param keysize_bits bit length of keydata bytes
 * @param oiv 16 byte IV value to write to start-up IV field.
 * @return NO_ERROR on success, error code otherwise.
 */
status_t se_set_device_aes_keyslot_and_iv(se_cdev_id_t device_id, uint32_t keyslot,
					  const uint8_t *keydata, uint32_t keysize_bits,
					  const uint8_t *oiv);

/**
 * @brief Set AES keyslot OIV only
 *
 * @param device_id identify device with SE_CDEV_* (enum se_cdev_id_e value)
 * @param keyslot index of keyslot to set [0..15] inclusive (SE AES keyslot)
 * @param oiv 16 byte IV value to write to start-up IV field.
 * @return NO_ERROR on success, error code otherwise.
 */
status_t se_set_device_aes_keyslot_iv(se_cdev_id_t device_id, uint32_t keyslot,
				      const uint8_t *oiv);
#endif /* HAVE_CCC_LIBRARY_MODE_CALLS */

/***************************************/

#endif /* HAVE_SE_AES */
#endif /* INCLUDED_CRYPTO_LIB_API_H */
