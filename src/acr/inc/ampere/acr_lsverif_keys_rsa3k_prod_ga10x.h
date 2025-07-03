/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_lsverif_keys_rsa3k_prod_ga10x.h
 */

#ifndef _ACR_LSVERIF_KEYS_RSA3K_PROD_GA10X_H_
#define _ACR_LSVERIF_KEYS_RSA3K_PROD_GA10X_H_
#include "acr.h"

//
// These arrays are public key components to help PKC-LS signature validation for GA10X.
// Debug public key is saved at acr\utilities\lskeys\ga10x\ga10x_rsa_3072_ls_pub_dbg.pem
// Debug private key is saved at acr\utilities\lskeys\ga10x\ga10x_rsa_3072_ls_pvt_dbg.pem
// Lwrrently we left production components to be empty because production key is not available.
//
#ifdef PKC_LS_RSA3K_KEYS_GA10X

    LwU32 g_rsaKeyModulusProd_GA10X[RSA_KEY_SIZE_3072_DWORD] ATTR_ALIGNED(ACR_PKC_LS_KEY_COMPONENT_ALIGNED_IN_BYTE) ATTR_OVLY(".data");
    LwU32 g_rsaKeyExponentProd_GA10X[RSA_KEY_SIZE_3072_DWORD] ATTR_ALIGNED(ACR_PKC_LS_KEY_COMPONENT_ALIGNED_IN_BYTE)  ATTR_OVLY(".data");
    LwU32 g_rsaKeyMpProd_GA10X[RSA_KEY_SIZE_3072_DWORD] ATTR_ALIGNED(ACR_PKC_LS_KEY_COMPONENT_ALIGNED_IN_BYTE)  ATTR_OVLY(".data");
    LwU32 g_rsaKeyRsqrProd_GA10X[RSA_KEY_SIZE_3072_DWORD] ATTR_ALIGNED(ACR_PKC_LS_KEY_COMPONENT_ALIGNED_IN_BYTE)  ATTR_OVLY(".data");

#endif // PKC_LS_RSA3K_KEYS_GA10X
#endif


