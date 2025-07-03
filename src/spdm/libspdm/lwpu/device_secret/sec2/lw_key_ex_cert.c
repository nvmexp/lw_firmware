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
 * @file   lw_key_ex_cert.c
 * @brief  Create and fill in KX certificate.
 */

 /* ------------------------- System Includes -------------------------------- */
#include "base.h"
#include "lwtypes.h"
#include "lw_utils.h"
/* ------------------------- Application Includes --------------------------- */
#include "lw_crypt.h"
#include "lw_device_secret.h"
#include "spdm_device_secret_lib.h"
#include "lw_apm_rts.h"
#include "seapi.h"

//
// We need this structure because including the 'x509_ak_nonca_cert.h' leads to
// double variable declaration
//
typedef struct Cert_Structure
{
     LwU16 length;
     LwU16 reserved;
     LwU8  root_hash[32];
     unsigned char Fmc_X509_Ak_Nonca_Cert_Ro_Data[1024];
} CERT_STRUCT;

CERT_STRUCT Key_Ex_Cert_Structure GCC_ATTRIB_SECTION("dmem_spdm", "_Key_Ex_Cert_Structure");

/**
  @brief Creates the KEY_EXCHANGE certificate by copying the AK certificate

  @param[out] ppKeyExCert         The pointer to the memory where the address of
                                  the certificate is written to
  @param[out] pKeyExCertChainSize The pointer to the memory where the size of
                                  the certificate is written to
  @param[in]  pAkCertChain        The pointer to the AK certificate chain
  @param[in]  akCertChainSize     The size of the AK certificate chain

  @return LW_TRUE if KX cert created successfully, LW_FALSE otherwise.
 */
boolean
spdm_generate_kx_certificate
(
    OUT uint8 **ppKeyExCertChain, 
    OUT uint32 *pKeyExCertChainSize,
    IN  uint8  *pAkCertChain,
    IN  uint32  akCertChainSize
)
{
    if (ppKeyExCertChain == NULL || pKeyExCertChainSize == NULL || pAkCertChain == NULL)
    {
        return LW_FALSE;
    }
    
    if (akCertChainSize > sizeof(Key_Ex_Cert_Structure))
    {
        return LW_FALSE;
    }

    copy_mem(&Key_Ex_Cert_Structure, pAkCertChain, akCertChainSize);

    *pKeyExCertChainSize = akCertChainSize;
    *ppKeyExCertChain = (LwU8 *) &Key_Ex_Cert_Structure;

    return LW_TRUE;
}