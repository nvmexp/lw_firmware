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
 * @file: acrse.h
 */

#ifndef _ACRRSA_H_
#define _ACRRSA_H_
#include <lwtypes.h>
#include "setypes.h"

//
// Enums for RSA parameters.
// c= x^y mod m
// SE_RSA_PARAMS_TYPE_MODULUS          : To set m
// SE_RSA_PARAMS_TYPE_MODULUS_EN       : To set m and also generate pre-computed parameters mp and r_sqr automatically.
// SE_RSA_PARAMS_TYPE_MONTGOMERY_MP    : To set mp
// SE_RSA_PARAMS_TYPE_MONTGOMERY_R_SQR : To set r_sqr
// SE_RSA_PARAMS_TYPE_EXPONENT         : To set y
// SE_RSA_PARAMS_TYPE_BASE             : To set x
//
typedef enum _SE_RSA_PARAMS_TYPE
{
    SE_RSA_PARAMS_TYPE_MODULUS               = 0,
    SE_RSA_PARAMS_TYPE_MODULUS_MONTGOMERY_EN = 1,
    SE_RSA_PARAMS_TYPE_MONTGOMERY_MP         = 2,
    SE_RSA_PARAMS_TYPE_MONTGOMERY_R_SQR      = 3,
    SE_RSA_PARAMS_TYPE_EXPONENT              = 4,
    SE_RSA_PARAMS_TYPE_BASE                  = 5,
    SE_RSA_PARAMS_TYPE_RSA_RESULT            = 6,
} SE_RSA_PARAMS_TYPE;

#define SE_ENGINE_OPERATION_TIMEOUT_DEFAULT (0xA0000000)  // sync with PTIMER, unit is ns

#define RSA_KEY_SIZE_3072_BIT               (3072)
#define RSA_KEY_SIZE_3072_ZERO_PADDING_BYTE (128)
#define RSA_KEY_SIZE_3072_BYTE              (RSA_KEY_SIZE_3072_BIT/8 + RSA_KEY_SIZE_3072_ZERO_PADDING_BYTE)
#define RSA_KEY_SIZE_3072_DWORD             (RSA_KEY_SIZE_3072_BYTE/4)

#define RSA_SIG_SIZE_3072_BIT               RSA_KEY_SIZE_3072_BIT
#define RSA_SIG_SIZE_3072_BYTE              (RSA_SIG_SIZE_3072_BIT >> 3)
#define RSA_SIG_SIZE_3072_DWORD             (RSA_SIG_SIZE_3072_BYTE >> 2) 

#endif

