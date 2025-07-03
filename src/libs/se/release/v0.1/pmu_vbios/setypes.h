/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  setypes.h
 * @brief Master header file defining types that may be used in the SE
 *        application. This file defines all types not already defined
 *        in lwtypes.h (from the SDK).
 */

#ifndef SETYPES_H
#define SETYPES_H

#include <lwtypes.h>

/*!
 * Defines a generic type that may be used to convey status information.  This
 * is very similar to the RM_STATUS type but smaller in width to save DMEM and
 * stack space.
 *
 * Available status values are defined in @ref seretval.h
 */ 

#define SE_MUTEX_TIMEOUT_VAL 0x100000
#define SE_PKA_FLAGS_DEFAULT 0x0
#define SE_MIN_RADIX         256
#define SE_RADIX_256         256
#define SE_RADIX_512         512

typedef LwU8 SE_STATUS;

typedef enum _SE_ALGORITHM
{
    RSA_512        = (0x0),
    RSA_1024       = (0x1),
} SE_ALGORITHM;

typedef enum _SE_MUTEX_TIMEOUT_ACTION
{
    INTERRUPT           = (0x0),
    RELEASE_MUTEX       = (0x1),
    RESET_PKA           = (0x2),
} SE_MUTEX_TIMEOUT_ACTION;

typedef enum _SE_FALCON_ID
{
    PMU   = (0x0),
    LWDEC = (0x1),
    DPU   = (0x2),
    SEC   = (0x3),
} SE_FALCON_ID;

typedef struct _def_rsa_params
{
    LwU32 *pModulus;  //modulus
    LwU32 *pBase;     //base
    LwU32 *pExponent; //exponent
} SE_RSA_PARAMS;

typedef struct _def_crypt_context
{
    SE_FALCON_ID            falconId;
    LwBool                  bUseKeyslot;
    LwU32                   keyslotIndex;
    LwU32                   keyslotPrivLevel;
    SE_ALGORITHM            algo;
    LwU32                   bitCount;
    SE_MUTEX_TIMEOUT_ACTION action;
    //struct for rsa parameters
    SE_RSA_PARAMS           rsaParams;
    //Pointer to return output of the operation
    LwU32                   *pOutput;
} SE_CRYPT_CONTEXT, *PSE_CRYPT_CONTEXT;

#ifndef MAX_PKA_SIZE_U32
#define MAX_PKA_SIZE_U32 (4096/32)
#endif

#define MAX_RSA_SIZE_WORDS 16
#define MAX_ECC_SIZE_WORDS 16

#define PKA_ADDR_OFFSET  0x4000

typedef struct _SE_PKA_REG
{
    LwU32 PKA_reg_words;
    LwU32 PKA_reg_bits;
    LwU32 PKA_radix_mask;
} SE_PKA_REG, *PSE_PKA_REG;

static const unsigned PKA_BANK_START_A          = PKA_ADDR_OFFSET + 0x400;
static const unsigned PKA_BANK_START_B          = PKA_ADDR_OFFSET + 0x800;
static const unsigned PKA_BANK_START_C          = PKA_ADDR_OFFSET + 0xC00;
static const unsigned PKA_BANK_START_D          = PKA_ADDR_OFFSET + 0x1000;

#endif // SETYPES_H
