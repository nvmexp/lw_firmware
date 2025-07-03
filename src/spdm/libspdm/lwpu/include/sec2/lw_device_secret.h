/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

// Included libspdm copyright, as file is LW-authored but uses libspdm code.

/*!
 * @file   lw_device_secret.h
 * @brief  Function declarations for device secret functionality, as
 *         well as defines and structs required for implementation.
 */

#ifndef _LW_DEVICE_SECRET_H_
#define _LW_DEVICE_SECRET_H_

/* ------------------------ Prototypes ------------------------------------- */
/**
  Generate AK key pair.

  @note Must be called before any libspdm processing is done, as AK is
  used in multiple scenarios when signing is required.

  @retval TRUE   key generation success.
  @retval FALSE  key generation failed.
**/
boolean spdm_device_secret_initialize_ak(void)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_device_secret_initialize_ak");

/**
  Generate KX key pair.

  @note Must be called before any libspdm processing is done, but after AK
        generation.
        APM-TODO PID2: Implement unique key generation for KX.

  @retval TRUE   key generation success.
  @retval FALSE  key generation failed.
**/
boolean spdm_device_secret_initialize_kx(void)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_device_secret_initialize_kx");

/**
  Get Public AK.

  @note This function returns the AK in big-endian format.

  @param  public_key       Buffer to receive Public AK.
  @param  public_key_size  Size in Bytes of buffer public_key points to.

  @retval TRUE   key generation success.
  @retval FALSE  output buffer for key too small.
  @retval FALSE  key generation failed.
**/
boolean spdm_device_secret_get_public_ak(OUT uint8 *public_key,
    IN OUT uintn *public_key_size)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_device_secret_get_public_ak");

/**
   @brief Derive the APM secret key used by the secure copy DMA and export it
          to global storage in a separate overlay
   
   @param context       Opaque (void) pointer to the context structure
   @param session_id    The current session ID

   @retval LW_TRUE      Operation successful
   @retval LW_FALSE     Operation failed
  
 */
boolean spdm_device_secret_derive_apm_sk(IN void *context, IN uint32 session_id)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_device_secret_derive_apm_sk");

/**
  @brief Completes the AK Certificate by:
             Filling in fields of the Certificate
             SHA 256 hash Certificate
             ECC Sign the Cert

  @param[in] pPoint  Pointer to coordinates, must have size of
                     ECC_P256_POINT_SIZE_IN_DWORDS.

  @return FLCN_OK if AK cert created successfully, relevant error otherwise.
 */
boolean spdm_generate_ak_certificate(uint8 **ppCertChain, uint32 *pCertChainSize)
    GCC_ATTRIB_SECTION("imem_spdm", "spdmGenerateAKCertificate");

/**
  @brief Creates the KX certificate by copying the AK certificate

  @param[out] ppKeyExCert         The pointer to the memory where the address of
                                  the certificate is written to
  @param[out] pKeyExCertChainSize The pointer to the memory where the size of
                                  the certificate is written to
  @param[in]  pAkCertChain        The pointer to the AK certificate chain
  @param[in]  akCertChainSize     The size of the AK certificate chain

  @return LW_TRUE if KX cert created successfully, LW_FALSE otherwise.
 */
boolean spdm_generate_kx_certificate(OUT uint8 **ppKeyExCertChain, OUT uint32 *pKeyExCertChainSize,
                                      IN uint8  *pAkCertChain, IN uint32  akCertChainSize)
    GCC_ATTRIB_SECTION("imem_spdm", "spdm_generate_kx_certificate");

#endif // _LW_DEVICE_SECRET_H_
