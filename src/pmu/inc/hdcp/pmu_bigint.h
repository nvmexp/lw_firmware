/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_bigint.h
 * @brief   Library to perform big integer arithmetic.
 */

#ifndef PMU_BIGINT_H_
#define PMU_BIGINT_H_

#include "flcntypes.h"

// Big integer size constants
#define BIGINT_MAX_DW   32
#define DSA_MODULUS_DW  32
#define DSA_DIVISOR_DW  5


typedef struct _BigIntModulus
{
    LwU32 digits;
    LwU32 ilwdigit;
    LwU32 n[BIGINT_MAX_DW];
    LwU32 r2[BIGINT_MAX_DW];
} BigIntModulus;


/* Function prototypes */
int  bigIntAdd(LwU32 *pZ, const LwU32 *pX, const LwU32 *pY, const LwU32 digits);
int  bigIntSubtract(LwU32 *pZ, const LwU32 *pX, const LwU32 *pY, const LwU32 digits);
int  bigIntCompare(const LwU32 *pX, const LwU32 *pY, const LwU32 digits);
void bigIntHalve(LwU32 *pZ, const LwU32 *pX, const LwU32 carry, const LwU32 digits);
void bigIntAddMod(LwU32 *pZ, const LwU32 *pX, const LwU32 *pY, const BigIntModulus *pP);
void bigIntSubtractMod(LwU32 *pZ, const LwU32 *pX, const LwU32 *pY, const BigIntModulus *pP);
void bigIntHalveMod(LwU32 *pZ, const LwU32 *pX, const BigIntModulus *pP);
void bigIntDoubleMod(LwU32 *pZ, const LwU32 *pX, const BigIntModulus *pP);
void bigIntIlwerseMod(LwU32 *pZ, const LwU32 *pX, const BigIntModulus *pP);
void bigIntMontgomeryProduct(LwU32 *pZ, const LwU32 *pX, const LwU32 *pY, const BigIntModulus *pP);
void bigIntMultiplyMod(LwU32 *pZ, const LwU32 *pX, const LwU32 *pY, const BigIntModulus *pP);
void bigIntMod(LwU32 *pZ, const LwU32 *pX, const BigIntModulus *pP, const LwU32 x_digits);
void bigIntPowerMod(LwU32 *pZ, const LwU32 *pX, const LwU32 *pE, const BigIntModulus *pP, const LwU32 e_digits);

#endif // PMU_BIGINT_H_
