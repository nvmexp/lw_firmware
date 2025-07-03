/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_bigint.c
 * @brief   Library to perform big integer arithmetic.
 *
 * All big integers are stored in little endian format in 32-bit 'digits'.
 * For example, the 128-bit integer 0xFFEEDDCCBBAA99887766554433221100 would
 * be stored as follows:
 *
 * @code
 *      X[0] = 0x33221100
 *      X[1] = 0x77665544
 *      X[2] = 0xBBAA9988
 *      X[3] = 0xFFEEDDCC
 * @endcode
 *
 * It does not matter whether the 32-bit values are stored in little or big
 * endian format themselves.
 *
 * In addition, the functions below refer to "digits." This refers to the
 * number of 32-bit values that make up the big integer. In the example above,
 * the integer has 4 digits.
 */

#include "flcntypes.h"
#include "pmusw.h"
#include "hdcp/pmu_bigint.h"


/*!
 * @brief Returns the value of the designated bit.
 *
 * @param[in]  pX
 *      Pointer to a big integer.
 *
 * @param[in]  i
 *      Bit index
 *
 * @returns X & (1 << i)
 */

static LwU32
bigIntGetBit
(
    const LwU32 *pX,
    LwU32 i
)
{
    return ((pX[i >> 5] >> (i & 0x1f)) & 1);
}


/*!
 * @brief Adds two big unsigned integers such that Z = X + Y.
 *
 * @pre X and Y must contain the same number of digits. If not, the smaller
 * value must be padded with leading zeros.
 *
 * @param[out] pZ
 *      Pointer to the allocated space for the sum.
 *
 * @param[in]  pX
 *      Pointer to the first term.
 *
 * @param[in]  pY
 *      Pointer to the second term.
 *
 * @param[in]  digits
 *      The number of digits to add.
 *
 * @returns The carry bit. If two unsigned integers of the same size were to
 *      be added together, this would indicate an overflow oclwrred.
 *
 * @sa bigIntSubtract
 */

int
bigIntAdd
(
    LwU32       *pZ,
    const LwU32 *pX,
    const LwU32 *pY,
    const LwU32  digits
)
{
    LwU32 i;
    LwU32 j;
    LwU32 k;

    for (i = k = 0; i < digits; i ++)
    {
        j = pX[i] + k;
        k = (j < k) ? 1 : 0;
        j += pY[i];
        k += (j < pY[i]) ? 1 : 0;
        pZ[i] = j;
    }

    return k;
}


/*!
 * @brief Subtracts two big unsigned integers such that Z = X - Y.
 *
 * @pre X and Y must contain the same number of digits. If not, the smaller
 * value must be padded with leading zeros.
 *
 * @param[out] pZ
 *      Pointer to the allocated space for the result.
 *
 * @param[in]  pX
 *      Pointer to the first term.
 *
 * @param[in]  pY
 *      Pointer to the second term.
 *
 * @param[in]  digits
 *      The number of digits to subtract.
 *
 * @returns The borrow bit. If two unsigned integers of the same size were to
 *      be subtracted, this would indicate a borrow was needed (i.e. Z > X).
 *
 * @sa bigIntAdd
 */

int
bigIntSubtract
(
    LwU32       *pZ,
    const LwU32 *pX,
    const LwU32 *pY,
    const LwU32  digits
)
{
    LwU32 i;
    LwU32 j;
    LwU32 k;

    for (i = k = 0; i < digits; i ++)
    {
        j = pX[i] - k;
        k = (j > (~k)) ? 1 : 0;
        j -= pY[i];
        k += (j > (~pY[i])) ? 1 : 0;
        pZ[i] = j;
    }

    return k;
}


/*!
 * @brief Compares two big integers for equality (X == Y).
 *
 * @param[in]  pX
 *      Pointer to the first value.
 *
 * @param[in]  pY
 *      Pointer to the second value.
 *
 * @param[in]  digits
 *      Number of digits to compare.
 *
 * @returns -1, 0, 1 based on whether X is less than, equal to, or greater than
 *      Y, respectively.
 */

int
bigIntCompare
(
    const LwU32 *pX,
    const LwU32 *pY,
    const LwU32  digits
)
{
    int i;

    for (i = digits - 1; i >= 0; i--)
    {
        if (pX[i] > pY[i])
        {
            return 1;
        }
        else if (pX[i] < pY[i])
        {
            return -1;
        }
    }

    return 0;
}


/*!
 * @brief Halves a big integer such that Z = X / 2.
 *
 * @param[out] pZ
 *      Pointer to the allocated space for the result.
 *
 * @param[in]  pX
 *      Pointer to the value to halve.
 *
 * @param[in]  carry
 *      If the carry bit is set, it will be used in the callwlation.
 *
 * @param[in]  digits
 *      Number of bits to halve.
 */

void
bigIntHalve
(
    LwU32 *pZ,
    const LwU32 *pX,
    const LwU32 carry,
    const LwU32 digits
)
{
    LwU32 i;

    for (i = 0; i < digits - 1; i++)
    {
        pZ[i] = (pX[i+1] << 31) | (pX[i] >> 1);
    }
    pZ[i] = (carry << 31) | (pX[i] >> 1);
}


/*!
 * @brief Adds two big unsigned integers with modulus such that
 * Z = (X + Y) \% P.
 *
 * @pre X and Y must contain the same number of digits. If not, the smaller
 * value must be padded with leading zeros.
 *
 * @param[out] pZ
 *      Pointer to the allocated space for the result.
 *
 * @param[in]  pX
 *      Pointer to the first addition term.
 *
 * @param[in]  pY
 *      Pointer to the second addition term.
 *
 * @param[in]  pP
 *      Pointer to the modulus.
 */

void
bigIntAddMod
(
    LwU32               *pZ,
    const LwU32         *pX,
    const LwU32         *pY,
    const BigIntModulus *pP
)
{
    if (bigIntAdd(pZ, pX, pY, pP->digits) ||
        bigIntCompare(pZ, pP->n, pP->digits) >= 0)
    {
        bigIntSubtract(pZ, pZ, pP->n, pP->digits);
    }
}


/*!
 * @brief Subtracts two big unsigned integers with modulus such that
 * Z = (X - Y) \% P.
 *
 * @pre X and Y must contain the same number of digits. If not, the smaller
 * value must be padded with leading zeros.
 *
 * @param[out] pZ
 *      Pointer to the allocated space for the result.
 *
 * @param[in]  pX
 *      Pointer to the first term.
 *
 * @param[in]  pY
 *      Pointer to the second term.
 *
 * @param[in] pP
 *      Pointer to the modulus.
 */

void
bigIntSubtractMod
(
    LwU32 *pZ,
    const LwU32 *pX,
    const LwU32 *pY,
    const BigIntModulus* pP
)
{
    if (bigIntSubtract(pZ, pX, pY, pP->digits))
    {
        bigIntAdd(pZ, pZ, pP->n, pP->digits);
    }
}


/*!
 * @brief Halves a big integer with modulus such that Z = (X / 2) \% P.
 *
 * @param[out] pZ
 *      Pointer to the allocated space for the result.
 *
 * @param[in]  pX
 *      Pointer to the integer to halve.
 *
 * @param[in]  pP
 *      Pointer to the modulus.
 */

void
bigIntHalveMod
(
    LwU32 *pZ,
    const LwU32 *pX,
    const BigIntModulus *pP
)
{
    LwU32 carry;

    if (*pX & 1)
    {
        carry = bigIntAdd(pZ, pX, pP->n, pP->digits);
        bigIntHalve(pZ, pZ, carry, pP->digits);
    }
    else
    {
        bigIntHalve(pZ, pZ, 0, pP->digits);
    }
}


/*!
 * @brief Doubles a big integer with modulus such that Z = (X * 2) \% P.
 *
 * @param[out] pZ
 *      Pointer to the allocated space for the result.
 *
 * @param[in]  pX
 *      Pointer to the integer to double.
 *
 * @param[in]  pP
 *      Pointer to the modulus.
 */

void
bigIntDoubleMod
(
    LwU32 *pZ,
    const LwU32 *pX,
    const BigIntModulus *pP
)
{
    LwU32 i;
    LwU32 j;
    LwU32 k;

    for (i = k = 0; i < pP->digits; i++)
    {
        j = (pX[i] >> 31);
        pZ[i] = (pX[i] << 1) | k;
        k = j;
    }
    if (k || bigIntCompare(pZ, pP->n, pP->digits) >= 0)
    {
        bigIntSubtract(pZ, pZ, pP->n, pP->digits);
    }
}


/*!
 * @brief Callwlates the ilwerse modulus such that Z = (X<sup>-1</sup>) \% P.
 *
 * @param[out] pZ
 *      Pointer to the allocated space for the result.
 *
 * @param[in]  pX
 *      Pointer to the base value.
 *
 * @param[in]  pP
 *      Pointer to the modulus.
 */

void
bigIntIlwerseMod
(
    LwU32 *pZ,
    const LwU32 *pX,
    const BigIntModulus *pP
)
{
    LwU32 b[BIGINT_MAX_DW] = {0};
    LwU32 d[BIGINT_MAX_DW] = {1};
    LwU32 u[BIGINT_MAX_DW];
    LwU32 v[BIGINT_MAX_DW];
    LwU32 zero[BIGINT_MAX_DW] = {0};

    memcpy(u, pP->n, pP->digits * sizeof(LwU32));
    memcpy(v, pX, pP->digits * sizeof(LwU32));
    while (bigIntCompare(u, zero, pP->digits))
    {
        while ((*u & 1) == 0)
        {
            bigIntHalveMod(u, u, pP);
            bigIntHalveMod(b, b, pP);
        }
        while ((*v & 1) == 0)
        {
            bigIntHalveMod(v, v, pP);
            bigIntHalveMod(d, d, pP);
        }
        if (bigIntCompare(u, v, pP->digits) >= 0)
        {
            bigIntSubtractMod(u, u, v, pP);
            bigIntSubtractMod(b, b, d, pP);
        }
        else
        {
            bigIntSubtractMod(v, v, u, pP);
            bigIntSubtractMod(d, d, b, pP);
        }
    }
    memcpy(pZ, d, pP->digits * sizeof(LwU32));
}


/*!
 * @brief Work-around function for binIntMontgomeryProduct.
 *
 * As per bug #726658, bigIntMontgomeryProduct() returns the incorrect values
 * when these for loops are in the function. Breaking out the inner for loops
 * cause the function to return the correct value.
 *
 * This function must _not_ be static, as the compiler will handle the code
 * differently, and the function will return the incorrect value.
 *
 * @param[in,out] k
 *      Intermediate result of a pY digit and pX, including carries.
 * @param[in]     pY
 *      Multiplicand made up multiple 32-bit digits
 * @param[in,out] t
 *      The result of the multiplication. Also contains results from previous
 *      multiplications.
 * @param[in]     pX
 *      32-bit multiplier
 * @param[in]     digits
 *      The number of digits in pY
 */

void
bigIntMontgomeryProduct_multiply(
    LwU64 *k,
    const LwU32 *pY,
    LwU32 *t,
    LwU32 pX,
    LwU32 digits)
{
    LwU32 j;

    for (j = 0; j < digits; j++)
    {
        *k = (LwU64) pY[j] * pX + t[j] + (*k >> 32);
        t[j] = (LwU32)(*k & 0xffffffff);
    }
}


/*!
 * @brief Work-around function for binIntMontgomeryProduct.
 *
 * As per bug #726658, bigIntMontgomeryProduct() returns the incorrect values
 * when these for loops are in the function. Breaking out the inner for loops
 * cause the function to return the correct value.
 *
 * This function must _not_ be static, as the compiler will handle the code
 * differently, and the function will return the incorrect value.
 *
 * @param[in,out] k
 *      Intermediate result of a pY digit and pX, including carries.
 * @param[in,out] t
 *      The result of the reduction. Also contains results from previous
 *      reductions.
 * @param[in]     n
 *      The Montgomery constant, made up of 32-bit digits.
 * @param[in]     m
 *      ilwdigits.
 * @param[in]     digits
 *      The number of digits in the Montgomery constant.
 */

void
bigIntMontgomeryProduct_reduce(
    LwU64 *k,
    LwU32 *t,
    const LwU32 *n,
    LwU32 m,
    LwU32 digits)
{
    LwU32 j;

    for (j = 1; j < digits; j++)
    {
        *k = (LwU64) m * n[j] + t[j] + (*k >> 32);
        t[j - 1] = (LwU32)(*k & 0xffffffff);
    }
}

/*!
 * @brief Product with modulus such that Z = (X * Y) \% P.
 *
 * @param[out] pZ
 *      Pointer to the allocated space for the result.
 *
 * @param[in]  pX
 *      Pointer to the first factor.
 *
 * @param[in]  pY
 *      Pointer to the second factor.
 *
 * @param[in]  pP
 *      Pointer to the modulus.
 */

void
bigIntMontgomeryProduct
(
    LwU32 *pZ,
    const LwU32 *pX,
    const LwU32 *pY,
    const BigIntModulus* pP
)
{
    static LwU32 digits;
    static LwU32 t[DSA_MODULUS_DW + 2];
    static const LwU32 *pN;
    LwU32 i;
    LwU32 m;
    LwU64 k = 0;

    digits = pP->digits;
    pN     = (LwU32*)&pP->n[0];

    memset(t, 0, (digits + 2) * sizeof(LwU32));
    for (i = 0; i < digits; i++)
    {
        k &= 0xffffffff;

        bigIntMontgomeryProduct_multiply(&k, pY, t, pX[i], digits);

        k  = (LwU64)t[digits] + (k >> 32);
        t[digits]     = (LwU32)(k & 0xffffffff);
        t[digits + 1] = (LwU32)(k >> 32);
        m             = (LwU32)(t[0] * pP->ilwdigit);
        k  = (LwU64)m * pP->n[0] + t[0];

        bigIntMontgomeryProduct_reduce(&k, t, pP->n, m, digits);

        k  = (LwU64)t[digits] + (k >> 32);
        t[digits - 1] = (LwU32)(k & 0xffffffff);
        t[digits]     = t[digits + 1] + (LwU32)(k >> 32);
    }

    if (t[digits] || bigIntCompare(t, pN, digits) >= 0)
    {
        bigIntSubtract(pZ, t, pN, digits);
    }
    else
    {
        memcpy(pZ, t, digits * sizeof(LwU32));
    }
}


/*!
 * @brief Big integer multiplication such that Z = X * Y.
 *
 * @param[out] pZ
 *      Pointer to the allocated space for the result.
 *
 * @param[in]  pX
 *      Pointer to the first factor.
 *
 * @param[in]  pY
 *      Pointer to the second factor.
 *
 * @param[in]  pP
 *      Pointer to the modulus.
 */

void
bigIntMultiplyMod
(
    LwU32 *pZ,
    const LwU32 *pX,
    const LwU32 *pY,
    const BigIntModulus *pP
)
{
    LwU32 t[BIGINT_MAX_DW];

    bigIntMontgomeryProduct(t, pX, pY, pP);
    bigIntMontgomeryProduct(pZ, t, pP->r2, pP);
}


/*!
 * @brief Big integer modulus such that Z = X \% P.
 *
 * @param[out] pZ
 *      Pointer to the allocated space for the result.
 *
 * @param[in]  pX
 *      Pointer to the input.
 *
 * @param[in]  pP
 *      Pointer to the modulus.
 *
 * @param[in]  x_digits
 *      The number of digits in X.
 */

void
bigIntMod
(
    LwU32 *pZ,
    const LwU32 *pX,
    const BigIntModulus *pP,
    const LwU32 x_digits
)
{
    LwU32 i;
    LwU32 j;

    i = (x_digits - 1) / pP->digits * pP->digits;

    memcpy(pZ, pX + i, (x_digits - i) * sizeof(LwU32));
    memset(pZ + x_digits - i, 0, (pP->digits - x_digits + i) * sizeof(LwU32));
    for (; i; i -= pP->digits)
    {
        bigIntMontgomeryProduct(pZ, pZ, pP->r2, pP);
        bigIntAddMod(pZ, pZ, pX + i - pP->digits, pP);
    }
    for (i = 0; bigIntCompare(pZ, pP->n, pP->digits) >= 0; i++)
    {
        if (*pZ & 1)
        {
            bigIntSubtract(pZ, pZ, pP->n, pP->digits);
        }
        bigIntHalve(pZ, pZ, 0, pP->digits);
    }
    for (j = 0; j < i; j++)
    {
        bigIntDoubleMod(pZ, pZ, pP);
    }
}


/*!
 * @brief Big integer exponential with modulus such that Z = (X<sup>E</sup>)
 * \% P.
 *
 * @param[out] pZ
 *      Pointer to the allocated space for the result.
 *
 * @param[in]  pX
 *      Pointer to the base.
 *
 * @param[in]  pE
 *      Pointer to the exponent.
 *
 * @param[in]  pP
 *      Pointer to the modulus.
 *
 * @param[in]  e_digits
 *      The number of digits in the exponent.
 */

void
bigIntPowerMod
(
    LwU32 *pZ,
    const LwU32 *pX,
    const LwU32 *pE,
    const BigIntModulus *pP,
    const LwU32 e_digits
)
{
    int i;

    LwU32 one[BIGINT_MAX_DW] = {1};
    LwU32 rx[BIGINT_MAX_DW];

    for (i = e_digits * 32 - 1; (i >= 0) && !bigIntGetBit(pE, i); i--)
    {
        // Do Nothing.  Must be a compound statement
    }
    if (i < 0)
    {
        memcpy(pZ, one, pP->digits * sizeof(LwU32));
        return;
    }
    bigIntMontgomeryProduct(rx, pX, pP->r2, pP);
    memcpy(pZ, rx, pP->digits * sizeof(LwU32));
    for (i--; i >= 0; i--)
    {
        bigIntMontgomeryProduct(pZ, pZ, pZ, pP);
        if (bigIntGetBit(pE, i))
        {
            bigIntMontgomeryProduct(pZ, pZ, rx, pP);
        }
    }
    bigIntMontgomeryProduct(pZ, pZ, one, pP);
}
