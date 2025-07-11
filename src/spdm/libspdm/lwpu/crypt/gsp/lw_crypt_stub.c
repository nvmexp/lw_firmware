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

// Included libSPDM copyright, as file is LW-authored but uses libSPDM code.

/*!
 * @file   lw_crypt_stub.c
 * @brief  Stub implementations for unused libSPDM cryptographic functionality.
 */

/* ------------------------- System Includes -------------------------------- */
#include "base.h"
#include "library/spdm_crypt_lib.h"

/* ------------------------- Public Functions ------------------------------- */
//
// APM-TODO CONFCOMP-394: Determine if we should remove below stubs,
// and comment out usage instead.
//
/**
  Retrieve the EC public key from one DER-encoded X509 certificate.

  @param[in]  cert         Pointer to the DER-encoded X509 certificate.
  @param[in]  cert_size     size of the X509 certificate in bytes.
  @param[out] ec_context    Pointer to new-generated EC DSA context which contain the retrieved
                           EC public key component. Use ec_free() function to free the
                           resource.

  If cert is NULL, then return FALSE;.
  If ec_context is NULL, then return FALSE;.

  @retval  TRUE   EC public key was retrieved successfully.
  @retval  FALSE  Fail to retrieve EC public key from X509 certificate.

**/
boolean ec_get_public_key_from_x509(IN const uint8 *cert, IN uintn cert_size,
                    OUT void **ec_context)
{
    return FALSE;
}

/**
  Get one X509 certificate from cert_chain.

  @param[in]      cert_chain         One or more ASN.1 DER-encoded X.509 certificates
                                    where the first certificate is signed by the Root
                                    Certificate or is the Root Cerificate itself. and
                                    subsequent cerificate is signed by the preceding
                                    cerificate.
  @param[in]      cert_chain_length   Total length of the certificate chain, in bytes.

  @param[in]      cert_index         index of certificate. If index is -1 indecate the
                                    last certificate in cert_chain.

  @param[out]     cert              The certificate at the index of cert_chain.
  @param[out]     cert_length        The length certificate at the index of cert_chain.

  @retval  TRUE   Success.
  @retval  FALSE  Failed to get certificate from certificate chain.
**/
boolean x509_get_cert_from_cert_chain(IN uint8 *cert_chain,
                      IN uintn cert_chain_length,
                      IN int32 cert_index, OUT uint8 **cert,
                      OUT uintn *cert_length)
{
    return FALSE;
}

/**
  This function returns the SPDM requester asymmetric algorithm size.

  @param  req_base_asym_alg               SPDM req_base_asym_alg

  @return SPDM requester asymmetric algorithm size.
**/
uint32 spdm_get_req_asym_signature_size(IN uint16 req_base_asym_alg)
{
    return 0;
}
/**
  Retrieve the asymmetric public key from one DER-encoded X509 certificate,
  based upon negotiated requester asymmetric algorithm.

  @param  req_base_asym_alg               SPDM req_base_asym_alg
  @param  cert                         Pointer to the DER-encoded X509 certificate.
  @param  cert_size                     size of the X509 certificate in bytes.
  @param  context                      Pointer to new-generated asymmetric context which contain the retrieved public key component.
                                       Use spdm_asym_free() function to free the resource.

  @retval  TRUE   public key was retrieved successfully.
  @retval  FALSE  Fail to retrieve public key from X509 certificate.
**/
boolean spdm_req_asym_get_public_key_from_x509(IN uint16 req_base_asym_alg,
                 IN const uint8 *cert,
                 IN uintn cert_size,
                 OUT void **context)
{
    return FALSE;
}

/**
  Release the specified asymmetric context,
  based upon negotiated requester asymmetric algorithm.

  @param  req_base_asym_alg               SPDM req_base_asym_alg
  @param  context                      Pointer to the asymmetric context to be released.
**/
void spdm_req_asym_free(IN uint16 req_base_asym_alg, IN void *context)
{
    return;
}

/**
  Verifies the asymmetric signature,
  based upon negotiated requester asymmetric algorithm.

  @param  req_base_asym_alg               SPDM req_base_asym_alg
  @param  bash_hash_algo                 SPDM bash_hash_algo
  @param  context                      Pointer to asymmetric context for signature verification.
  @param  message                      Pointer to octet message to be checked (before hash).
  @param  message_size                  size of the message in bytes.
  @param  signature                    Pointer to asymmetric signature to be verified.
  @param  sig_size                      size of signature in bytes.

  @retval  TRUE   Valid asymmetric signature.
  @retval  FALSE  Invalid asymmetric signature or invalid asymmetric context.
**/
boolean spdm_req_asym_verify(IN uint16 req_base_asym_alg,
           IN uint32 bash_hash_algo, IN void *context,
           IN const uint8 *message, IN uintn message_size,
           IN const uint8 *signature, IN uintn sig_size)
{
    return FALSE;
}

/**
  This function verifies the integrity of certificate chain buffer including spdm_cert_chain_t header.

  @param  bash_hash_algo                 SPDM bash_hash_algo
  @param  cert_chain_buffer              The certificate chain buffer including spdm_cert_chain_t header.
  @param  cert_chain_buffer_size          size in bytes of the certificate chain buffer.

  @retval TRUE  certificate chain buffer integrity verification pass.
  @retval FALSE certificate chain buffer integrity verification fail.
**/
boolean spdm_verify_certificate_chain_buffer
(
    IN uint32 bash_hash_algo,
    IN void *cert_chain_buffer,
    IN uintn cert_chain_buffer_size
)
{
    return FALSE;
}

/**
  Retrieve the asymmetric public key from one DER-encoded X509 certificate,
  based upon negotiated asymmetric algorithm.

  @param  base_asym_algo               SPDM base_asym_algo
  @param  cert                         Pointer to the DER-encoded X509 certificate.
  @param  cert_size                    size of the X509 certificate in bytes.
  @param  context                      Pointer to new-generated asymmetric context which contain the retrieved public key component.
                                       Use spdm_asym_free() function to free the resource.

  @retval  TRUE   public key was retrieved successfully.
  @retval  FALSE  Fail to retrieve public key from X509 certificate.
**/
boolean spdm_asym_get_public_key_from_x509
(
    IN uint32 base_asym_algo,
	IN const uint8 *cert,
    IN uintn cert_size,
    OUT void **context
)
{
    return TRUE;
}

