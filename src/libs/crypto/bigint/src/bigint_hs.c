/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    bigint_hs.c
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
#include "flcnifcmn.h"
#include "lwuproc.h"
#include "mem_hs.h"
#include "bigint_hs.h"
#include "csb.h"
#include "common_hslib.h"

/* ------------------------- Function Prototypes ---------------------------- */
static void _libBigIntHsEntry(void)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "start")
    GCC_ATTRIB_USED();
static LwU32 _bigIntGetBit_hs(const LwU32 *pX, LwU32 i)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "_bigIntGetBit_hs");
static void _bigIntSwapEndianness_hs(LwU8 *pOutput, const LwU8 *pInput, LwU32 size)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "_bigIntSwapEndianness_hs");
static LwU32 _bigIntIlwerseDigit_hs(LwU32 x)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "_bigIntIlwerseDigit_hs");

static LwU64 _mulu32_hs(LwU32 a, LwU32 b)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "_mulu32_hs");
static LwU32 _mulu32trunc_hs(LwU32 a, LwU32 b)
    GCC_ATTRIB_SECTION("imem_libBigIntHs", "_mulu32trunc_hs");
/* ------------------------- Private Functions ------------------------------ */
/*!
 * _libBigIntHsEntry
 *
 * @brief Entry function of Big Int library. This function merely returns. It
 *        is used to validate the overlay blocks corresponding to the secure
 *        overlay before jumping to arbitrary functions inside it. The linker
 *        script should make sure that it puts this section at the top of the
 *        overlay to ensure that we can jump to the overlay's 0th offset before
 *        exelwting code at arbitrary offsets inside the overlay.
 */
static void
_libBigIntHsEntry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();

    return;
}

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
_bigIntGetBit_hs
(
    const LwU32 *pX,
    LwU32 i
)
{
    return ((pX[i >> 5] >> (i & 0x1f)) & 1);
}

/*!
 * @brief Swaps the endianness of the buffer provided. Works in-place or
 *        out-of-place.
 *
 * @param[out]  pOutput  Buffer to store the resulting swap.
 * @param[in]   pInput   Buffer to swap the endianness of.
 * @param[in]   size     The size of the buffer to swap.
 */
static void
_bigIntSwapEndianness_hs
(
    LwU8 *pOutput,
    const LwU8 *pInput,
    LwU32 size
)
{
    LwU32 i;

    for (i = 0; i < size / 2; i++)
    {
        LwU8 val1 = pInput[i];
        LwU8 val2 = pInput[size - 1 - i];
        pOutput[i]           = val2;
        pOutput[size - 1- i] = val1;
    }
}

/*!
 * Find the ilwerse modulo 2^32. Input must be an odd number.
 *
 * @param[in]  x  Odd number to find the ilwerse modulo 2^32.
 *
 * @return Ilwerse modulo 2^32 of the number specified.
 */
static LwU32
_bigIntIlwerseDigit_hs
(
    LwU32 x
)
{
    LwU32 i = 1;
    LwU32 j = 2;

    do
    {
        i ^= (j ^ (j & _mulu32trunc_hs(x, i)));
        j += j;
    } while (0 != j);

    return i;
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Initializes a BigIntModulus with the data provided.
 *
 * @param[out] pModulus  BigIntModulus to initialize.
 * @param[in]  pData     Data buffer to initialize the data buffer with.
 * @param[in]  size      Size of the data buffer.
 */
void
bigIntModulusInit_hs
(
    BigIntModulus *pModulus,
    const LwU8    *pData,
    LwU32          size
)
{
    LwU32 i;

    memset_hs(pModulus, 0, sizeof(BigIntModulus));

    // Initialize digits
    pModulus->digits = (size + sizeof(LwU32) - 1) / sizeof(LwU32);

    // Initialize pModulus->n
    _bigIntSwapEndianness_hs((LwU8*)pModulus->n, pData, size);

    // Callwlate pModulus->n
    pModulus->ilwdigit = _bigIntIlwerseDigit_hs(pModulus->n[0]);

    // Callwlate pModulus->r2
    pModulus->r2[0] = 1;
    for (i = 0; i < (pModulus->digits << 6); i++)
    {
        bigIntDoubleMod_hs(pModulus->r2, pModulus->r2, pModulus);
    }
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
 * @sa bigIntSubtract_hs
 */
int
bigIntAdd_hs
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
 * @sa bigIntAdd_hs
 */
int
bigIntSubtract_hs
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
bigIntCompare_hs
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
bigIntHalve_hs
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
bigIntAddMod_hs
(
    LwU32               *pZ,
    const LwU32         *pX,
    const LwU32         *pY,
    const BigIntModulus *pP
)
{
    if (bigIntAdd_hs(pZ, pX, pY, pP->digits) ||
        bigIntCompare_hs(pZ, pP->n, pP->digits) >= 0)
    {
        bigIntSubtract_hs(pZ, pZ, pP->n, pP->digits);
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
bigIntSubtractMod_hs
(
    LwU32 *pZ,
    const LwU32 *pX,
    const LwU32 *pY,
    const BigIntModulus* pP
)
{
    if (bigIntSubtract_hs(pZ, pX, pY, pP->digits))
    {
        bigIntAdd_hs(pZ, pZ, pP->n, pP->digits);
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
bigIntHalveMod_hs
(
    LwU32 *pZ,
    const LwU32 *pX,
    const BigIntModulus *pP
)
{
    LwU32 carry;

    if (*pX & 1)
    {
        carry = bigIntAdd_hs(pZ, pX, pP->n, pP->digits);
        bigIntHalve_hs(pZ, pZ, carry, pP->digits);
    }
    else
    {
        bigIntHalve_hs(pZ, pZ, 0, pP->digits);
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
bigIntDoubleMod_hs
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
    if (k || bigIntCompare_hs(pZ, pP->n, pP->digits) >= 0)
    {
        bigIntSubtract_hs(pZ, pZ, pP->n, pP->digits);
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
bigIntIlwerseMod_hs
(
    LwU32 *pZ,
    const LwU32 *pX,
    const BigIntModulus *pP
)
{
    LwU32 b[BIGINT_MAX_DW];     /* Initialized to {0} below */
    LwU32 d[BIGINT_MAX_DW];     /* Initialized to {1} below */
    LwU32 u[BIGINT_MAX_DW];
    LwU32 v[BIGINT_MAX_DW];
    LwU32 zero[BIGINT_MAX_DW];  /* Initialized to {0} below */

    memset_hs(b, 0, sizeof(b));

    d[0] = 1;
    memset_hs(&d[1], 0, sizeof(d) - sizeof(d[0]));
    memset_hs(zero, 0, sizeof(zero));

    memcpy_hs(u, pP->n, pP->digits << 2);   /* copy pP->digits * sizeof(LwU32) bytes */
    memcpy_hs(v, pX, pP->digits << 2);      /* copy pP->digits * sizeof(LwU32) bytes */
    while (bigIntCompare_hs(u, zero, pP->digits))
    {
        while ((*u & 1) == 0)
        {
            bigIntHalveMod_hs(u, u, pP);
            bigIntHalveMod_hs(b, b, pP);
        }
        while ((*v & 1) == 0)
        {
            bigIntHalveMod_hs(v, v, pP);
            bigIntHalveMod_hs(d, d, pP);
        }
        if (bigIntCompare_hs(u, v, pP->digits) >= 0)
        {
            bigIntSubtractMod_hs(u, u, v, pP);
            bigIntSubtractMod_hs(b, b, d, pP);
        }
        else
        {
            bigIntSubtractMod_hs(v, v, u, pP);
            bigIntSubtractMod_hs(d, d, b, pP);
        }
    }
    memcpy_hs(pZ, d, pP->digits << 2); /* copy pP->digits * sizeof(LwU32) bytes */
}

/*!
 * @brief Work-around function for binIntMontgomeryProduct.
 *
 * As per bug #726658, bigIntMontgomeryProduct_hs() returns the incorrect values
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
bigIntMontgomeryProduct_multiply_hs(
    LwU64 *k,
    const LwU32 *pY,
    LwU32 *t,
    LwU32 pX,
    LwU32 digits)
{
    LwU32 j;

    for (j = 0; j < digits; j++)
    {
        *k = _mulu32_hs(pY[j], pX) + t[j] + (*k >> 32);
        t[j] = (LwU32)(*k & 0xffffffff);
    }
}

/*!
 * @brief Work-around function for binIntMontgomeryProduct.
 *
 * As per bug #726658, bigIntMontgomeryProduct_hs() returns the incorrect values
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
bigIntMontgomeryProduct_reduce_hs(
    LwU64 *k,
    LwU32 *t,
    const LwU32 *n,
    LwU32 m,
    LwU32 digits)
{
    LwU32 j;

    for (j = 1; j < digits; j++)
    {
        *k = _mulu32_hs(m, n[j]) + t[j] + (*k >> 32);
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
bigIntMontgomeryProduct_hs
(
    LwU32 *pZ,
    const LwU32 *pX,
    const LwU32 *pY,
    const BigIntModulus* pP
)
{
    static LwU32 digits;
    static LwU32 t[BIGINT_MAX_DW + 2];
    static const LwU32 *pN;
    LwU32 i;
    LwU32 m;
    LwU64 k = 0;

    digits = pP->digits;
    pN     = (LwU32*)&pP->n[0];

    memset_hs(t, 0, (digits + 2) << 2);     /* Set (digits + 2) * sizeof(LwU32) bytes */
    for (i = 0; i < digits; i++)
    {
        k &= 0xffffffff;

        bigIntMontgomeryProduct_multiply_hs(&k, pY, t, pX[i], digits);

        k  = (LwU64)t[digits] + (k >> 32);
        t[digits]     = (LwU32)(k & 0xffffffff);
        t[digits + 1] = (LwU32)(k >> 32);
        m             = _mulu32trunc_hs(t[0], pP->ilwdigit);
        k             = _mulu32_hs(m, pP->n[0]) + t[0];

        bigIntMontgomeryProduct_reduce_hs(&k, t, pP->n, m, digits);

        k  = (LwU64)t[digits] + (k >> 32);
        t[digits - 1] = (LwU32)(k & 0xffffffff);
        t[digits]     = t[digits + 1] + (LwU32)(k >> 32);
    }

    if (t[digits] || bigIntCompare_hs(t, pN, digits) >= 0)
    {
        bigIntSubtract_hs(pZ, t, pN, digits);
    }
    else
    {
        memcpy_hs(pZ, t, digits << 2);      /* Copy digits * sizeof(LwU32) bytes */
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
bigIntMultiplyMod_hs
(
    LwU32 *pZ,
    const LwU32 *pX,
    const LwU32 *pY,
    const BigIntModulus *pP
)
{
    LwU32 t[BIGINT_MAX_DW];

    bigIntMontgomeryProduct_hs(t, pX, pY, pP);
    bigIntMontgomeryProduct_hs(pZ, t, pP->r2, pP);
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
bigIntMod_hs
(
    LwU32 *pZ,
    const LwU32 *pX,
    const BigIntModulus *pP,
    const LwU32 x_digits
)
{
    LwU32 i;
    LwU32 j;

    i = _mulu32trunc_hs((x_digits - 1) / pP->digits, pP->digits);

    memcpy_hs(pZ, pX + i, (x_digits - i) << 2);     /* Copy (x_digits - i) * sizeof(LwU32) bytes */
    memset_hs(pZ + x_digits - i, 0, (pP->digits - x_digits + i) << 2); /* Copy ((pP->digits - x_digits + i) * sizeof(LwU32) bytes */
    for (; i; i -= pP->digits)
    {
        bigIntMontgomeryProduct_hs(pZ, pZ, pP->r2, pP);
        bigIntAddMod_hs(pZ, pZ, pX + i - pP->digits, pP);
    }
    for (i = 0; bigIntCompare_hs(pZ, pP->n, pP->digits) >= 0; i++)
    {
        if (*pZ & 1)
        {
            bigIntSubtract_hs(pZ, pZ, pP->n, pP->digits);
        }
        bigIntHalve_hs(pZ, pZ, 0, pP->digits);
    }
    for (j = 0; j < i; j++)
    {
        bigIntDoubleMod_hs(pZ, pZ, pP);
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
bigIntPowerMod_hs
(
    LwU32 *pZ,
    const LwU32 *pX,
    const LwU32 *pE,
    const BigIntModulus *pP,
    const LwU32 e_digits
)
{
    int i;

    LwU32 one[BIGINT_MAX_DW];   /* Initialized to {1} below */
    LwU32 rx[BIGINT_MAX_DW];

    one[0] = 1;
    memset_hs(&one[1], 0, sizeof(one) - sizeof(one[0]));

    /* Start from i = e_digits * 32 - 1 */
    for (i = (e_digits << 5) - 1; (i >= 0) && !_bigIntGetBit_hs(pE, i); i--)
    {
        // Do Nothing.  Must be a compound statement
    }
    if (i < 0)
    {
        memcpy_hs(pZ, one, pP->digits << 2);     /* Copy pP->digits * sizeof(LwU32) bytes */
        return;
    }
    bigIntMontgomeryProduct_hs(rx, pX, pP->r2, pP);
    memcpy_hs(pZ, rx, pP->digits << 2);         /* Copy pP->digits * sizeof(LwU32) bytes */
    for (i--; i >= 0; i--)
    {
        bigIntMontgomeryProduct_hs(pZ, pZ, pZ, pP);
        if (_bigIntGetBit_hs(pE, i))
        {
            bigIntMontgomeryProduct_hs(pZ, pZ, rx, pP);
        }
    }
    bigIntMontgomeryProduct_hs(pZ, pZ, one, pP);
}

/*!
 * @brief Returns the product of two 32-bit unsigned integers
 *        truncated to 32-bit.
 *
 * @param[in]  a
 *      The first term as a 32-bit unsigned integer
 *
 * @param[in]  b
 *      The second term as a 32-bit unsigned integer
 *
 * @returns The result of a * b as a 32-bit unsigned integer
 */
static LwU32
_mulu32trunc_hs
(
    LwU32 a,
    LwU32 b
)
{
    LwU16 a1, a2;
    LwU16 b1, b2;
    LwU32 a2b2, a1b2, a2b1;
    LwU32 result;

    /*
     * Falcon has a 16-bit multiplication instruction.
     * Break down the 32-bit multiplication into 16-bit operations.
     */
    a1 = a >> 16;
    a2 = a & 0xffff;

    b1 = b >> 16;
    b2 = b & 0xffff;

    a2b2 = a2 * b2;
    a1b2 = a1 * b2;
    a2b1 = a2 * b1;

    result = (LwU32)a2b2 + (((LwU32)a1b2) << 16) +
            (((LwU32)a2b1) << 16);
    return result;
}

/*!
 * @brief Returns the product of two 32-bit unsigned integers
 *        in a 64-bit unsigned integer.
 *
 * @param[in]  a
 *      The first term as a 32-bit unsigned integer
 *
 * @param[in]  b
 *      The second term as a 32-bit unsigned integer
 *
 * @returns The result of a * b as a 64-bit unsigned integer
 *
 */
static LwU64
_mulu32_hs(LwU32 a, LwU32 b)
{
    LwU16 a1, a2;
    LwU16 b1, b2;
    LwU32 a2b2, a1b2, a2b1, a1b1;
    LwU64 result;

    /*
     * Falcon has a 16-bit multiplication instruction.
     * Break down the 32-bit multiplication into 16-bit operations.
     */
    a1 = a >> 16;
    a2 = a & 0xffff;

    b1 = b >> 16;
    b2 = b & 0xffff;

    a2b2 = a2 * b2;
    a1b2 = a1 * b2;
    a2b1 = a2 * b1;
    a1b1 = a1 * b1;

    result = (LwU64)a2b2 + ((LwU64)a1b2 << 16) +
            ((LwU64)a2b1 << 16) + ((LwU64)a1b1 << 32);
    return result;
}
