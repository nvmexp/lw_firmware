/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_lsverif_keys_rsa3k_prod_gh100.h
 */

#ifndef _ACR_LSVERIF_KEYS_RSA3K_PROD_GH100_H_
#define _ACR_LSVERIF_KEYS_RSA3K_PROD_GH100_H_
#include "acr.h"

//
// These arrays are public key components to help PKC-LS signature validation for GH100.
// Prod key is to be changed in GH100: Bug 200724489
//
#ifdef PKC_LS_RSA3K_KEYS_GH100
    LwU32 g_rsaKeyModulusProd_GH100[RSA_KEY_SIZE_3072_DWORD] ATTR_ALIGNED(ACR_PKC_LS_KEY_COMPONENT_ALIGNED_IN_BYTE) ATTR_OVLY(".data");
    LwU8 g_rsaKeyExponentProd_GH100[4] ATTR_OVLY(".data");
#endif // PKC_LS_RSA3K_KEYS_GH100

#endif // _ACR_LSVERIF_KEYS_RSA3K_PROD_GH100_H_


