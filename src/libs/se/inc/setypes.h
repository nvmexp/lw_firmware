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


//
// Mutex timeout defines
//
#define SE_MUTEX_WATCHDOG_TIMEOUT_VAL_USEC 0x100000
#define SE_MUTEX_TIMEOUT_VAL_USEC          0x100000

//
// Defines for key sizes.
// ECC also supports a 521 bit key.  This key size requires some special casing, so capping 
// at 512 for now.
//
#define NUM_BITS_IN_DWORD       32
#define MAX_RSA_KEY_SIZE        4096
#define MAX_ECC_KEY_SIZE        512    
#define MAX_RSA_SIZE_DWORDS     ((MAX_RSA_KEY_SIZE + (NUM_BITS_IN_DWORD - 1)) / NUM_BITS_IN_DWORD)
#define MAX_ECC_SIZE_DWORDS     ((MAX_ECC_KEY_SIZE + (NUM_BITS_IN_DWORD - 1)) / NUM_BITS_IN_DWORD)

//
// Defines for read/write error checking.
//
#define SE_CSB_PRIV_PRI_ERROR_MASK                   0xFFFF0000
#define SE_CSB_PRIV_PRI_ERROR_CODE                   0xBADF0000

//
// Defines SE status return values.  
//
typedef enum _SE_STATUS
{
    SE_OK                                      = 0x00,
    SE_ERR_BAD_INPUT_DATA                      = 0x01,
    SE_ERR_POINT_NOT_ON_LWRVE                  = 0x02,
    SE_ERR_PKA_OP_ILWALID_OPCODE               = 0x03,
    SE_ERR_PKA_OP_F_STACK_UNDERFLOW            = 0x05,
    SE_ERR_PKA_OP_F_STACK_OVERFLOW             = 0x06,
    SE_ERR_PKA_OP_WATCHDOG                     = 0x07,
    SE_ERR_PKA_OP_HOST_REQUEST                 = 0x08,
    SE_ERR_PKA_OP_P_STACK_UNDERFLOW            = 0x09,
    SE_ERR_PKA_OP_P_STACK_OVERFLOW             = 0x0A,
    SE_ERR_PKA_OP_MEM_PORT_COLLISION           = 0x0B,
    SE_ERR_PKA_OP_OPERATION_SIZE_EXCEED_CFG    = 0x0C,
    SE_ERR_PKA_OP_UNKNOWN_ERR                  = 0x0D,
    SE_ERR_NOT_SUPPORTED                       = 0x0E,
    SE_ERR_ILWALID_ARGUMENT                    = 0x0F,
    SE_ERR_ILLEGAL_OPERATION                   = 0x10,
    SE_ERR_TIMEOUT                             = 0x11,
    SE_ERR_NO_FREE_MEM                         = 0x12,
    SE_ERR_MUTEX_ACQUIRE                       = 0x13,
    SE_ERR_MUTEX_ID_NOT_AVAILABLE              = 0x14,
    SE_ERR_MORE_PROCESSING_REQUIRED            = 0x15,
    SE_ERR_FAILED_ECDSA_ATTEMPTS               = 0x16,
    SE_ERR_ILWALID_KEY_SIZE                    = 0x17,
    SE_ERR_TRNG_R256_NOT_ENABLED               = 0x18,
    SE_ERR_SECBUS_READ_ERROR                   = 0x19,
    SE_ERR_SECBUS_WRITE_ERROR                  = 0x1A,
    SE_ERR_CSB_PRIV_READ_ERROR                 = 0x1B,
    SE_ERR_CSB_PRIV_READ_0xBADF_VALUE_RECEIVED = 0x1C,
    SE_ERR_CSB_PRIV_WRITE_ERROR                = 0x1D,
    SE_ERROR                                   = 0xFF
} SE_STATUS;

//
// This is a combination of key sizes supported for all SE operations.
// A key size of 521 is supported,  but looks to require some special casing
// so not added yet.
//
typedef enum _SE_KEY_SIZE
{
    SE_KEY_SIZE_160  = 160,
    SE_KEY_SIZE_192  = 192,
    SE_KEY_SIZE_224  = 224,
    SE_KEY_SIZE_256  = 256,
    SE_KEY_SIZE_384  = 384,
    SE_KEY_SIZE_512  = 512,
    SE_KEY_SIZE_768  = 768,
    SE_KEY_SIZE_1024 = 1024,
    SE_KEY_SIZE_1536 = 1536,
    SE_KEY_SIZE_2048 = 2048,
    SE_KEY_SIZE_3072 = 3072,
    SE_KEY_SIZE_4096 = 4096,
} SE_KEY_SIZE;

//
// Struct containing the number of words of operand memory 
// based on the key length (radix)
//
typedef struct _SE_PKA_REG
{
    LwU32 pkaKeySizeDW;
    LwU32 pkaRadixMask;
} SE_PKA_REG, *PSE_PKA_REG;

//
// Enums for addressing operand memory
//
typedef enum _SE_PKA_BANK
{
    SE_PKA_BANK_A = 0,
    SE_PKA_BANK_B = 1,
    SE_PKA_BANK_C = 2,
    SE_PKA_BANK_D = 3,
} SE_PKA_BANK;


#ifndef NULL
#define NULL  0
#endif

#endif // SETYPES_H
