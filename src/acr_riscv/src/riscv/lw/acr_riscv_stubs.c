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
 * @file: acr_riscv_stubs.c
 */

#include "acr.h"
#include "acr_objacr.h"
#include "config/g_acr_private.h"
#include "config/g_hal_register.h"

void acrCallwlateDmhash_GH100
(
    LwU8           *pHash,
    LwU8           *pData,
    LwU32          size
)
{
}

ACR_STATUS acrDeriveLsVerifKeyAndEncryptDmHash_GH100
(
    LwU8   *pSaltBuf,  //16B aligned
    LwU8   *pDmHash,   //16B aligned
    LwU32  falconId,
    LwBool bUseFalconId
)
{
    return ACR_OK;
}

