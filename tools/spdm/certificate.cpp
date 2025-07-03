/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Including libspdm copyright, as functions & descriptions were copied from libspdm-source.

/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

#include "spdm_cppintfc.h"

//
// We are stubbing out most certificate functionality. This is because we simply
// care about retrieving public key from certificate, and not valiating the
// rest of the fields, as that is out of scope of the LWPU SPDM implementations.
//

/**
  Retrieve the subject bytes from one X.509 certificate.

  If cert is NULL, then return FALSE.
  If subject_size is NULL, then return FALSE.
  If this interface is not supported, then return FALSE.

  @param[in]      cert         Pointer to the DER-encoded X509 certificate.
  @param[in]      cert_size     size of the X509 certificate in bytes.
  @param[out]     cert_subject  Pointer to the retrieved certificate subject bytes.
  @param[in, out] subject_size  The size in bytes of the cert_subject buffer on input,
                               and the size of buffer returned cert_subject on output.

  @retval  TRUE   The certificate subject retrieved successfully.
  @retval  FALSE  Invalid certificate, or the subject_size is too small for the result.
                  The subject_size will be updated with the required size.
  @retval  FALSE  This interface is not supported.

**/
boolean x509_get_subject_name(IN const uint8 *cert, IN uintn cert_size,
                  OUT uint8 *cert_subject,
                  IN OUT uintn *subject_size)
{
    return true;
}

/**
  Retrieve the version from one X.509 certificate.

  If cert is NULL, then return FALSE.
  If cert_size is 0, then return FALSE.
  If this interface is not supported, then return FALSE.

  @param[in]      cert         Pointer to the DER-encoded X509 certificate.
  @param[in]      cert_size     size of the X509 certificate in bytes.
  @param[out]     version      Pointer to the retrieved version integer.

  @retval RETURN_SUCCESS           The certificate version retrieved successfully.
  @retval RETURN_ILWALID_PARAMETER If  cert is NULL or cert_size is Zero.
  @retval RETURN_UNSUPPORTED       The operation is not supported.

**/
return_status x509_get_version(IN const uint8 *cert, IN uintn cert_size,
                   OUT uintn *version)
{
    if (version == nullptr)
    {
        return RETURN_ILWALID_PARAMETER;
    }

    // spdm_x509_certificate_check looks for the following value.
    *version = 0x2;
    return RETURN_SUCCESS;
}

/**
  Retrieve the serialNumber from one X.509 certificate.

  If cert is NULL, then return FALSE.
  If cert_size is 0, then return FALSE.
  If this interface is not supported, then return FALSE.

  @param[in]      cert         Pointer to the DER-encoded X509 certificate.
  @param[in]      cert_size     size of the X509 certificate in bytes.
  @param[out]     serial_number  Pointer to the retrieved certificate serial_number bytes.
  @param[in, out] serial_number_size  The size in bytes of the serial_number buffer on input,
                               and the size of buffer returned serial_number on output.

  @retval RETURN_SUCCESS           The certificate serialNumber retrieved successfully.
  @retval RETURN_ILWALID_PARAMETER If cert is NULL or cert_size is Zero.
                                   If serial_number_size is NULL.
                                   If Certificate is invalid.
  @retval RETURN_NOT_FOUND         If no serial_number exists.
  @retval RETURN_BUFFER_TOO_SMALL  If the serial_number is NULL. The required buffer size
                                   (including the final null) is returned in the
                                   serial_number_size parameter.
  @retval RETURN_UNSUPPORTED       The operation is not supported.
**/
return_status x509_get_serial_number(IN const uint8 *cert, IN uintn cert_size,
                     OUT uint8 *serial_number,
                     OPTIONAL IN OUT uintn *serial_number_size)
{
    // For some reason, spdm_x509_certificate_check considers RETURN_BUFFER_TOO_SMALL success.
    return RETURN_BUFFER_TOO_SMALL;
}

/**
  Retrieve the signature algorithm from one X.509 certificate.

  @param[in]      cert             Pointer to the DER-encoded X509 certificate.
  @param[in]      cert_size         size of the X509 certificate in bytes.
  @param[out]     oid              signature algorithm Object identifier buffer
  @param[in,out]  oid_size          signature algorithm Object identifier buffer size

  @retval RETURN_SUCCESS           The certificate Extension data retrieved successfully.
  @retval RETURN_ILWALID_PARAMETER If cert is NULL.
                                   If oid_size is NULL.
                                   If oid is not NULL and *oid_size is 0.
                                   If Certificate is invalid.
  @retval RETURN_NOT_FOUND         If no SignatureType.
  @retval RETURN_BUFFER_TOO_SMALL  If the oid is NULL. The required buffer size
                                   is returned in the oid_size.
  @retval RETURN_UNSUPPORTED       The operation is not supported.
**/
return_status x509_get_signature_algorithm(IN const uint8 *cert,
                       IN uintn cert_size, OUT uint8 *oid,
                       OPTIONAL IN OUT uintn *oid_size)
{
    // For some reason, spdm_x509_certificate_check considers RETURN_BUFFER_TOO_SMALL success.
    return RETURN_BUFFER_TOO_SMALL;
}

/**
  Retrieve the issuer bytes from one X.509 certificate.

  If cert is NULL, then return FALSE.
  If CertIssuerSize is NULL, then return FALSE.
  If this interface is not supported, then return FALSE.

  @param[in]      cert         Pointer to the DER-encoded X509 certificate.
  @param[in]      cert_size     size of the X509 certificate in bytes.
  @param[out]     CertIssuer  Pointer to the retrieved certificate subject bytes.
  @param[in, out] CertIssuerSize  The size in bytes of the CertIssuer buffer on input,
                               and the size of buffer returned cert_subject on output.

  @retval  TRUE   The certificate issuer retrieved successfully.
  @retval  FALSE  Invalid certificate, or the CertIssuerSize is too small for the result.
                  The CertIssuerSize will be updated with the required size.
  @retval  FALSE  This interface is not supported.

**/
boolean x509_get_issuer_name(IN const uint8 *cert, IN uintn cert_size,
                 OUT uint8 *CertIssuer,
                 IN OUT uintn *CertIssuerSize)
{
    // spdm_x509_certificate_check just looks for success here.
    return true;
}

/**
  Retrieve the Validity from one X.509 certificate

  If cert is NULL, then return FALSE.
  If CertIssuerSize is NULL, then return FALSE.
  If this interface is not supported, then return FALSE.

  @param[in]      cert         Pointer to the DER-encoded X509 certificate.
  @param[in]      cert_size     size of the X509 certificate in bytes.
  @param[out]     from         notBefore Pointer to date_time object.
  @param[in,out]  from_size     notBefore date_time object size.
  @param[out]     to           notAfter Pointer to date_time object.
  @param[in,out]  to_size       notAfter date_time object size.

  Note: x509_compare_date_time to compare date_time oject
        x509SetDateTime to get a date_time object from a date_time_str

  @retval  TRUE   The certificate Validity retrieved successfully.
  @retval  FALSE  Invalid certificate, or Validity retrieve failed.
  @retval  FALSE  This interface is not supported.
**/
boolean x509_get_validity(IN const uint8 *cert, IN uintn cert_size,
              IN uint8 *from, IN OUT uintn *from_size, IN uint8 *to,
              IN OUT uintn *to_size)
{
    // spdm_x509_certificate_check just looks for success here.
    return true;
}

/**
  format a date_time object into DataTime buffer

  If date_time_str is NULL, then return FALSE.
  If date_time_size is NULL, then return FALSE.
  If this interface is not supported, then return FALSE.

  @param[in]      date_time_str      date_time string like YYYYMMDDhhmmssZ
                                   Ref: https://www.w3.org/TR/NOTE-datetime
                                   Z stand for UTC time
  @param[out]     date_time         Pointer to a date_time object.
  @param[in,out]  date_time_size     date_time object buffer size.

  @retval RETURN_SUCCESS           The date_time object create successfully.
  @retval RETURN_ILWALID_PARAMETER If date_time_str is NULL.
                                   If date_time_size is NULL.
                                   If date_time is not NULL and *date_time_size is 0.
                                   If year month day hour minute second combination is invalid datetime.
  @retval RETURN_BUFFER_TOO_SMALL  If the date_time is NULL. The required buffer size
                                   (including the final null) is returned in the
                                   date_time_size parameter.
  @retval RETURN_UNSUPPORTED       The operation is not supported.
**/
return_status x509_set_date_time(char8 *date_time_str, IN OUT void *date_time,
                 IN OUT uintn *date_time_size)
{
    // spdm_x509_certificate_check just looks for success here.
    return RETURN_SUCCESS;
}

/**
  Compare date_time1 object and date_time2 object.

  If date_time1 is NULL, then return -2.
  If date_time2 is NULL, then return -2.
  If date_time1 == date_time2, then return 0
  If date_time1 > date_time2, then return 1
  If date_time1 < date_time2, then return -1

  @param[in]      date_time1         Pointer to a date_time Ojbect
  @param[in]      date_time2         Pointer to a date_time Object

  @retval  0      If date_time1 == date_time2
  @retval  1      If date_time1 > date_time2
  @retval  -1     If date_time1 < date_time2
**/
intn x509_compare_date_time(IN void *date_time1, IN void *date_time2)
{
    // spdm_x509_certificate_check just looks for success here.
    return 0;
}

/**
  Retrieve the key usage from one X.509 certificate.

  @param[in]      cert             Pointer to the DER-encoded X509 certificate.
  @param[in]      cert_size         size of the X509 certificate in bytes.
  @param[out]     usage            key usage (CRYPTO_X509_KU_*)

  @retval  TRUE   The certificate key usage retrieved successfully.
  @retval  FALSE  Invalid certificate, or usage is NULL
  @retval  FALSE  This interface is not supported.
**/
boolean x509_get_key_usage(IN const uint8 *cert, IN uintn cert_size,
               OUT uintn *usage)
{
    if (usage == nullptr)
    {
        return false;
    }

    // spdm_x509_certificate_check looks for the following usage.
    *usage = CRYPTO_X509_KU_DIGITAL_SIGNATURE;
    return true;
}

/**
  Retrieve the Extended key usage from one X.509 certificate.

  @param[in]      cert             Pointer to the DER-encoded X509 certificate.
  @param[in]      cert_size         size of the X509 certificate in bytes.
  @param[out]     usage            key usage bytes.
  @param[in, out] usage_size        key usage buffer sizs in bytes.

  @retval RETURN_SUCCESS           The usage bytes retrieve successfully.
  @retval RETURN_ILWALID_PARAMETER If cert is NULL.
                                   If cert_size is NULL.
                                   If usage is not NULL and *usage_size is 0.
                                   If cert is invalid.
  @retval RETURN_BUFFER_TOO_SMALL  If the usage is NULL. The required buffer size
                                   is returned in the usage_size parameter.
  @retval RETURN_UNSUPPORTED       The operation is not supported.
**/
return_status x509_get_extended_key_usage(IN const uint8 *cert,
                      IN uintn cert_size, OUT uint8 *usage,
                      IN OUT uintn *usage_size)   
{
    if (usage_size == nullptr)
    {
        return RETURN_ILWALID_PARAMETER;
    }

    // spdm_x509_certificate_check treats the following as success.
    *usage_size = 0;
    return RETURN_BUFFER_TOO_SMALL;
}

/**
  Verify one X509 certificate was issued by the trusted CA.

  @param[in]      cert_chain         One or more ASN.1 DER-encoded X.509 certificates
                                    where the first certificate is signed by the Root
                                    Certificate or is the Root Cerificate itself. and
                                    subsequent cerificate is signed by the preceding
                                    cerificate.
  @param[in]      cert_chain_length   Total length of the certificate chain, in bytes.

  @param[in]      root_cert          Trusted Root Certificate buffer

  @param[in]      root_cert_length    Trusted Root Certificate buffer length

  @retval  TRUE   All cerificates was issued by the first certificate in X509Certchain.
  @retval  FALSE  Invalid certificate or the certificate was not issued by the given
                  trusted CA.
**/
boolean x509_verify_cert_chain(IN uint8 *root_cert, IN uintn root_cert_length,
                   IN uint8 *cert_chain,
                   IN uintn cert_chain_length)
{
    return true;
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
    if (cert_chain == nullptr || cert_chain_length == 0 ||
        cert == nullptr || cert_length == nullptr)
    {
        return RETURN_ILWALID_PARAMETER;
    }

    //
    // TODO: For now, SPDM implementations only support one certificate.
    // Therefore, first certificate will be only certificate.
    // Update this function once we have whole chain.
    //
    *cert        = cert_chain;
    *cert_length = cert_chain_length;
    return true;
}

/**
  Retrieve the EC public key from one DER-encoded X509 certificate.

  @param[in]  cert         Pointer to the DER-encoded X509 certificate.
  @param[in]  cert_size     size of the X509 certificate in bytes.
  @param[out] ec_context    Pointer to new-generated EC DSA context which contain the retrieved
                           EC public key component. Use ec_free() function to free the
                           resource.

  If cert is NULL, then return FALSE.
  If ec_context is NULL, then return FALSE.

  @retval  TRUE   EC public key was retrieved successfully.
  @retval  FALSE  Fail to retrieve EC public key from X509 certificate.

**/
boolean ec_get_public_key_from_x509(IN const uint8 *cert, IN uintn cert_size,
                    OUT void **ec_context)
{
    if (cert == NULL || cert_size == 0 || ec_context == NULL)
    {
        return false;
    }

    //
    // TODO: CryptoPP does not provide method to parse certificate for
    // EC key, so we must handle with WAR. This is VERY hacky.
    // Do not check this in. But unblock further testing via this.
    //
    if (cert_size < 324)
    {
       return false;
    }

    *ec_context = ec_new_by_nid(CRYPTO_NID_SECP256R1);
    if (*ec_context == NULL)
    {
        return false;
    }

    return ec_set_pub_key(*ec_context, (uint8 *)cert + 260, 64);
}

/**
  Retrieve the EC Private key from the password-protected PEM key data.

  @param[in]  pem_data      Pointer to the PEM-encoded key data to be retrieved.
  @param[in]  pem_size      size of the PEM key data in bytes.
  @param[in]  password     NULL-terminated passphrase used for encrypted PEM key data.
  @param[out] ec_context    Pointer to new-generated EC DSA context which contain the retrieved
                           EC private key component. Use ec_free() function to free the
                           resource.

  If pem_data is NULL, then return FALSE.
  If ec_context is NULL, then return FALSE.

  @retval  TRUE   EC Private key was retrieved successfully.
  @retval  FALSE  Invalid PEM key data or incorrect password.

**/
boolean ec_get_private_key_from_pem(IN const uint8 *pem_data, IN uintn pem_size,
                    IN const char8 *password,
                    OUT void **ec_context)
{
    // Stub out as error, as we should never need this function.
    return FALSE;
}
