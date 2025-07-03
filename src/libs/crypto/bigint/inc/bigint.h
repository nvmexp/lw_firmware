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
 * @file    bigint.h
 * @brief   Library to perform big integer arithmetic.
 */

#ifndef BIGINT_H
#define BIGINT_H

// Big integer size constants
#define BIGINT_MAX_DW   96
#define DSA_DIVISOR_DW  5

/*!
 * Structure to hold Big integer Montgomery constants
 */
typedef struct _BigIntModulus
{
    LwU32 digits;
    LwU32 ilwdigit;
    LwU32 n[BIGINT_MAX_DW];
    LwU32 r2[BIGINT_MAX_DW];
} BigIntModulus;


/* Function prototypes */
void bigIntModulusInit(BigIntModulus *pModulus, const LwU8 *pData, LwU32 size)
    GCC_ATTRIB_SECTION("imem_libBigInt", "bigIntModulusInit");
int  bigIntAdd(LwU32 *pZ, const LwU32 *pX, const LwU32 *pY, const LwU32 digits)
    GCC_ATTRIB_SECTION("imem_libBigInt", "bigIntAdd");
int  bigIntSubtract(LwU32 *pZ, const LwU32 *pX, const LwU32 *pY, const LwU32 digits)
    GCC_ATTRIB_SECTION("imem_libBigInt", "bigIntSubtract");
int  bigIntCompare(const LwU32 *pX, const LwU32 *pY, const LwU32 digits)
    GCC_ATTRIB_SECTION("imem_libBigInt", "bigIntCompare");
void bigIntHalve(LwU32 *pZ, const LwU32 *pX, const LwU32 carry, const LwU32 digits)
    GCC_ATTRIB_SECTION("imem_libBigInt", "bigIntHalve");
void bigIntAddMod(LwU32 *pZ, const LwU32 *pX, const LwU32 *pY, const BigIntModulus *pP)
    GCC_ATTRIB_SECTION("imem_libBigInt", "bigIntAddMod");
void bigIntSubtractMod(LwU32 *pZ, const LwU32 *pX, const LwU32 *pY, const BigIntModulus *pP)
    GCC_ATTRIB_SECTION("imem_libBigInt", "bigIntSubtractMod");
void bigIntHalveMod(LwU32 *pZ, const LwU32 *pX, const BigIntModulus *pP)
    GCC_ATTRIB_SECTION("imem_libBigInt", "bigIntHalveMod");
void bigIntDoubleMod(LwU32 *pZ, const LwU32 *pX, const BigIntModulus *pP)
    GCC_ATTRIB_SECTION("imem_libBigInt", "bigIntDoubleMod");
void bigIntIlwerseMod(LwU32 *pZ, const LwU32 *pX, const BigIntModulus *pP)
    GCC_ATTRIB_SECTION("imem_libBigInt", "bigIntIlwerseMod");
void bigIntMontgomeryProduct(LwU32 *pZ, const LwU32 *pX, const LwU32 *pY, const BigIntModulus *pP)
    GCC_ATTRIB_SECTION("imem_libBigInt", "bigIntMontgomeryProduct");
void bigIntMontgomeryProduct_multiply(LwU64 *k, const LwU32 *pY, LwU32 *t, LwU32 pX, LwU32 digits)
    GCC_ATTRIB_SECTION("imem_libBigInt", "bigIntMontgomeryProduct_multiply");
void bigIntMontgomeryProduct_reduce(LwU64 *k, LwU32 *t, const LwU32 *n, LwU32 m, LwU32 digits)
    GCC_ATTRIB_SECTION("imem_libBigInt", "bigIntMontgomeryProduct_reduce");
void bigIntMultiplyMod(LwU32 *pZ, const LwU32 *pX, const LwU32 *pY, const BigIntModulus *pP)
    GCC_ATTRIB_SECTION("imem_libBigInt", "bigIntMultiplyMod");
void bigIntMod(LwU32 *pZ, const LwU32 *pX, const BigIntModulus *pP, const LwU32 x_digits)
    GCC_ATTRIB_SECTION("imem_libBigInt", "bigIntMod");
void bigIntPowerMod(LwU32 *pZ, const LwU32 *pX, const LwU32 *pE, const BigIntModulus *pP, const LwU32 e_digits)
    GCC_ATTRIB_SECTION("imem_libBigInt", "bigIntPowerMod");

#endif // BIGINT_H
