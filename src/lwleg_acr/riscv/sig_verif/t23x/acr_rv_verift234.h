/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef ACR_RV_VERIFT234_H
#define ACR_RV_VERIFT234_H

#include "tegra_se.h"

#define GET_MOST_SIGNIFICANT_BIT(keySize)      (keySize > 0 ? ((keySize - 1) & 7) : 0)
#define GET_ENC_MESSAGE_SIZE_BYTE(keySize)     (keySize + 7) >> 3;
#define PKCS1_MGF1_COUNTER_SIZE_BYTE           (4)
#define RSA_PSS_PADDING_ZEROS_SIZE_BYTE        (8)

#define acrVerifySignature_HAL(pSignature, binarysize, binOffset, pUcodeDesc) \
        acrVerifySignature_T234(pSignature, binarysize, binOffset, pUcodeDesc)

ACR_STATUS acrValidateSignature(uint8_t *sha256_hash, uint8_t *signature);
ACR_STATUS acrVerifySignature_T234(LwU8 *pSignature, LwU32 binarysize, LwU32 binOffset, PLSF_UCODE_DESC_V2 pUcodeDesc);

#endif // ACR_RV_VERIFT234_H