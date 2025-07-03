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
 * @file    bigint_hs.h
 * @brief   HS version of the library to perform big integer arithmetic.
 */

#ifndef BIGINT_HS_H
#define BIGINT_HS_H

#include "bigint.h"     // Import common data structures and constants

/* Function prototypes */
void bigIntModulusInit_hs(BigIntModulus *pModulus, const LwU8 *pData, LwU32 size)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "bigIntModulusInit_hs");
int  bigIntAdd_hs(LwU32 *pZ, const LwU32 *pX, const LwU32 *pY, const LwU32 digits)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "bigIntAdd_hs");
int  bigIntSubtract_hs(LwU32 *pZ, const LwU32 *pX, const LwU32 *pY, const LwU32 digits)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "bigIntSubtract_hs");
int  bigIntCompare_hs(const LwU32 *pX, const LwU32 *pY, const LwU32 digits)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "bigIntCompare_hs");
void bigIntHalve_hs(LwU32 *pZ, const LwU32 *pX, const LwU32 carry, const LwU32 digits)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "bigIntHalve_hs");
void bigIntAddMod_hs(LwU32 *pZ, const LwU32 *pX, const LwU32 *pY, const BigIntModulus *pP)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "bigIntAddMod_hs");
void bigIntSubtractMod_hs(LwU32 *pZ, const LwU32 *pX, const LwU32 *pY, const BigIntModulus *pP)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "bigIntSubtractMod_hs");
void bigIntHalveMod_hs(LwU32 *pZ, const LwU32 *pX, const BigIntModulus *pP)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "bigIntHalveMod_hs");
void bigIntDoubleMod_hs(LwU32 *pZ, const LwU32 *pX, const BigIntModulus *pP)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "bigIntDoubleMod_hs");
void bigIntIlwerseMod_hs(LwU32 *pZ, const LwU32 *pX, const BigIntModulus *pP)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "bigIntIlwerseMod_hs");
void bigIntMontgomeryProduct_hs(LwU32 *pZ, const LwU32 *pX, const LwU32 *pY, const BigIntModulus *pP)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "bigIntMontgomeryProduct_hs");
void bigIntMultiplyMod_hs(LwU32 *pZ, const LwU32 *pX, const LwU32 *pY, const BigIntModulus *pP)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "bigIntMultiplyMod_hs");
void bigIntMod_hs(LwU32 *pZ, const LwU32 *pX, const BigIntModulus *pP, const LwU32 x_digits)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "bigIntMod_hs");
void bigIntPowerMod_hs(LwU32 *pZ, const LwU32 *pX, const LwU32 *pE, const BigIntModulus *pP, const LwU32 e_digits)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "bigIntPowerMod_hs");
void bigIntMontgomeryProduct_multiply_hs(LwU64 *k, const LwU32 *pY, LwU32 *t, LwU32 pX, LwU32 digits)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "bigIntMontgomeryProduct_multiply_hs");
void bigIntMontgomeryProduct_reduce_hs(LwU64 *k, LwU32 *t, const LwU32 *n, LwU32 m, LwU32 digits)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "bigIntMontgomeryProduct_reduce_hs");

#endif // BIGINT_HS_H
