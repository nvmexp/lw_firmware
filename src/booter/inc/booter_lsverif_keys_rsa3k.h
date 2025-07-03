/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: booter_lsverif_keys_rsa3k_debug_ga10x.h
 */

#ifndef _BOOTER_LSVERIF_KEYS_RSA3K_H_
#define _BOOTER_LSVERIF_KEYS_RSA3K_H_

#include "booter.h"

LwU32 g_rsaKeyModulus[RSA_KEY_SIZE_3072_DWORD] ATTR_ALIGNED(BOOTER_PKC_LS_KEY_COMPONENT_ALIGNED_IN_BYTE)    ATTR_OVLY(".data");
LwU32 g_rsaKeyExponent[RSA_KEY_SIZE_3072_DWORD] ATTR_ALIGNED(BOOTER_PKC_LS_KEY_COMPONENT_ALIGNED_IN_BYTE)   ATTR_OVLY(".data");
LwU32 g_rsaKeyMp[RSA_KEY_SIZE_3072_DWORD] ATTR_ALIGNED(BOOTER_PKC_LS_KEY_COMPONENT_ALIGNED_IN_BYTE)         ATTR_OVLY(".data");
LwU32 g_rsaKeyRsqr[RSA_KEY_SIZE_3072_DWORD] ATTR_ALIGNED(BOOTER_PKC_LS_KEY_COMPONENT_ALIGNED_IN_BYTE)       ATTR_OVLY(".data");

#endif


