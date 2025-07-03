/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lib_fxp.c
 * @brief  Library of FXP Math Functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "lib/lib_fxp.h"
#include "lib_lw64.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Number of steps to use in Taylor series approximations of expFXP().
 *
 * Value was chosen by Taylor's theorem to provide error smaller
 * than FXP20.12 fractional unit (i.e. 1 / 0x1000 =~ 0.000244) for expFXP(x) in
 * range [0,1].  That is the value which satisfies the inequality:
 *
 *     4 / 0.000244 <= (k + 1)!
 *
 * http://en.wikipedia.org/wiki/Taylor%27s_theorem#Estimates_for_the_remainder
 */
#define EXP_TAYLOR_SERIES_NUM_STEPS                                            8

/*!
 * Lowest integer power of e which will overflow FXP20.12.
 */
#define E_INT_POWER_OVERFLOW_FXP20_12                                         14

/*!
 * Highest FXP20.12 power of e which will overflow FXP20.12.  This value is
 * computed as ln(0xFFFFFFFF / 0x1000) and floored to representation as
 * FXP20.12:
 *
 *    ln(0xFFFFFFFF / 0x1000) = 13.862944 ~ 13.862793
 */
#define E_MAX_POWER_FXP20_12                             (LwUFXP20_12)0x0000DDCE

/*!
 * Pre-computed values to use for reducing range of input to Gregory series.
 *
 * Reduce range of input to that which has an acceptable amount of error.  In
 * this case, reducing range from [1,2] -> [2^-0.2, 2^0.2].  This is
 * accomplished by implementing the following segmentations of the input range:
 *
 * [1, 2^0.2) -unity -> [1, 2^0.2)
 * [2^0.2, 2^0.6) -/2^0.4-> [2^-0.2, 2^0.2)
 * [2^0.6, 2^1] -/2^0.8-> [2^-0.2, 2^0.2]
 */
#define NUM_2_POW_0P2_UFXP20_12                          (LwUFXP20_12)0x00001261
#define NUM_2_POW_0P6_UFXP20_12                          (LwUFXP20_12)0x00001840
#define NUM_2_POW_M0P4_UFXP20_12                         (LwUFXP20_12)0x00000c20
#define NUM_2_POW_M0P8_UFXP20_12                         (LwUFXP20_12)0x00000930
#define LN_2_POW_M0P4_SFXP20_12                          (LwSFXP20_12)0x0000046f

/*!
 * Coefficients to use in rational polynomial evaluation of Gregory Series.
 *
 * The Gregory Series is as follows:
 *
 * ln(x) = ln((1 + z)/(1 - z)) =
 *     2 * ( z + z^3 + z^5 + z^7 + z^9 + ... )
 *               ---   ---   ---   ---
 *                3     5     7     9
 *
 *      Where z = (x - 1)/(x + 1)
 *
 * The 5-th order continued fraction of this series can be resolved to rational
 * polynomial of the form:
 *
 * ln(x) = 2z * ( a + b * z^2 )
 *         ------------------
 *              1 + c * z^2
 *
 *     Where a = 1
 *           b = -4/15 = -0.266667
 *           c = -9/15 = -0.6
 */
#define LN_GREGORY_A_SFXP20_12                           (LwSFXP20_12)0x00001000
#define LN_GREGORY_B_SFXP20_12                           (LwSFXP20_12)0xFFFFFBBC
#define LN_GREGORY_C_SFXP20_12                           (LwSFXP20_12)0xFFFFF666

/*!
 * Pre-computed natural log of 2 - i.e. ln(2) - in FXP20.12 format.
 */
#define LN2_FXP20_12                                     (LwUFXP20_12)0x00000B17

/* ------------------------- Static Variables ------------------------------- */
/*!
 * Pre-computed integer powers of e in FXP20.12 - up to maximum integer numbers
 * which can fit in FXP20.12 format.
 */
static LwUFXP20_12
_ePow[E_INT_POWER_OVERFLOW_FXP20_12]
    GCC_ATTRIB_SECTION("dmem_pmgr", "_ePow") =
{
    0x00001000, // expFXP(0)
    0x00002b7e, // expFXP(1)
    0x0000763a, // expFXP(2)
    0x0001415e, // expFXP(3)
    0x00036992, // expFXP(4)
    0x0009469c, // expFXP(5)
    0x001936dc, // expFXP(6)
    0x00448a21, // expFXP(7)
    0x00ba4f54, // expFXP(8)
    0x01fa7158, // expFXP(9)
    0x0560a774, // expFXP(10)
    0x0e9e2244, // expFXP(11)
    0x27bc2caa, // expFXP(12)
    0x6c02d646, // expFXP(13)
};

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/*!
 * Static helper functions for various FXP arithmetic, including some
 * transcendental functions.
 */
static LwUFXP20_12 s_expFXPTaylor(LwUFXP20_12 x);
static LwSFXP20_12 s_lnFXPGregory(LwUFXP20_12 x);
static LwSFXP20_12 s_lnFXPGregoryPoly(LwUFXP20_12 x);

/* ------------------------- Public Functions ------------------------------- */
LwSFXP32
multSFXP32
(
    LwU8     nFracBits,
    LwSFXP32 x,
    LwSFXP32 y
)
{
    LwSFXP32 result;
    LwBool   bSign = LW_FALSE;

    // Handling for negative sign.
    if (x < LW_TYPES_FXP_ZERO)
    {
        x *= -1;
        bSign = !bSign;
    }
    if (y < LW_TYPES_FXP_ZERO)
    {
        y *= -1;
        bSign = !bSign;
    }

    // Do the multiplication as unsigned.
    result = multUFXP32(nFracBits, (LwUFXP32)x, (LwUFXP32)y);

    // Correct for sign as necessary.
    if (bSign)
    {
        result *= -1;
    }

    return result;
}

/*!
 * Callwlates transcendental power function f(x,y) = x^y in FXP with various
 * optimizations for run-time and accuracy.
 *
 * @param[in] x   Base value
 * @param[in] y   Exponent value
 *
 * @return Callwlated result for x^y
 */
LwUFXP20_12
powFXP
(
    LwUFXP20_12 x,
    LwSFXP20_12 y
)
{
    LwUFXP20_12 result;
    LwBool      bNegative = LW_FALSE;

    // Short-circuit for x == 0 => 0
    if (x == LW_TYPES_FXP_ZERO)
    {
        return LW_TYPES_FXP_ZERO;
    }

    // Short-circuit for y == 0 => 1
    if (y == LW_TYPES_FXP_ZERO)
    {
        return LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1);
    }
    // Short-circuit for y == 1 => x
    if (y == LW_TYPES_S32_TO_SFXP_X_Y(20, 12, 1))
    {
        return x;
    }

    // If negative power, colwert to positive and just divide after.
    if (y < LW_TYPES_FXP_ZERO)
    {
        y *= -1;
        bNegative = LW_TRUE;
    }

    // Breakdown to x^y = e ^ (y * ln(x))
    result = expFXP(multSFXP20_12(y, lnFXP(x)));

    // If y was originally negative, divide 1 by result.
    if (bNegative)
    {
        result = LW_TYPES_U32_TO_UFXP_X_Y_SCALED(8, 24, 1, result);
    }

    return result;
}

/*!
 * Callwlates transcendental exponential function f(x) = e^x in FXP with various
 * optimizations for run-time and accuracy.
 *
 * @param[in] x
 *     Exponent value.  @note x must be in the range [-E_MAX_POWER_FXP20_12,
 *     E_MAX_POWER_FXP20_12] as larger values will overflow FXP 20.12.  Larger
 *     values will be truncated to that range with a PMU_BREAKPOINT.
 *
 * @return Callwlation of e^x
 */
LwUFXP20_12
expFXP
(
    LwSFXP20_12 x
)
{
    LwUFXP20_12 result;
    LwBool      bNegative = LW_FALSE;

    // If negative power, colwert to positive and just divide after.
    if (x < LW_TYPES_FXP_ZERO)
    {
        x *= -1;
        bNegative = LW_TRUE;
    }

    //
    // Sanity check that x is in acceptable range.  Otherwise, truncate to
    // largest acceptable value.
    //
    if (x > E_MAX_POWER_FXP20_12)
    {
        x = E_MAX_POWER_FXP20_12;
        PMU_BREAKPOINT();
    }

    //
    // Break out fractional part to handle by taylor series in range [0,1):
    //
    // e^(a + b) = e^a * e^b
    //
    result = multUFXP20_12(
                _ePow[LW_TYPES_UFXP_X_Y_TO_U32(20, 12, (LwUFXP20_12)x)],
                s_expFXPTaylor(DRF_VAL(_TYPES, _FXP, _FRACTIONAL(20, 12),
                            (LwUFXP20_12)x)));

    // If y was originally negative, divide 1 by result.
    if (bNegative)
    {
        result = LW_TYPES_U32_TO_UFXP_X_Y_SCALED(8, 24, 1, result);
    }

    return result;
}

/*!
 * Callwlates natural logarithm function f(x) = ln(x) in FXP with various
 * optimizations for run-time and accuracy.
 *
 * @param[in] x
 *     Value for which to find natural logarithm.  @note x must never be zero!
 *
 * @return Callwlation of ln(x).
 */
LwSFXP20_12
lnFXP
(
    LwUFXP20_12 x
)
{
    LwS32 n = 0;

    // ln(0) is undefined!
    PMU_HALT_COND(x != LW_TYPES_FXP_ZERO);

    //
    // Range reduction:
    //     x = 2^n * f
    //  => ln(x) = n*ln(2) + ln(f)
    //
    // Use this approach to reduce f into range [1,2].
    //

    // If <1, shift up to range [1,2].
    while (x < LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1))
    {
        x <<= 1;
        n--;
    }

    // If >2, shift down to range [1,2].
    while (x > LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 2))
    {
        x >>= 1;
        n++;
    }

    //
    // Callwlate the FXP estimation via Taylor Series in range [1,2] and then
    // add correction for range reduction.
    //
    return n * LN2_FXP20_12 + s_lnFXPGregory(x);
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Callwlates transcendental exponential function f(x) = e^x for FXP value, with
 * requirement x < 1.  Uses Horner Scheme to callwlate Taylor/Maclaurin Series
 * approximation with EXP_TAYLOR_SERIES_NUM_STEPS steps.
 *
 * Taylor Series for e^x =
 *
 *     Sum(n, 0, inf){ x^n / n! }
 *
 * Can unroll this with a Horner Scheme to be
 *
 *     a_{n} = x / n + 1
 *     a_{n - 1} = a_{n} * x / (n - 1) + 1
 *     ...
 *     a_{1} = a_{2} * x / 1 + 1
 *
 * http://en.wikipedia.org/wiki/Exponential_function#Formal_definition
 * http://en.wikipedia.org/wiki/Horner_scheme
 *
 * @param[in] x  Exponent value.  Must be in range [0, 1).
 *
 * @return Callwlation of e^x.
 */
static LwUFXP20_12
s_expFXPTaylor
(
    LwUFXP20_12 x
)
{
    LwU32       i;
    LwUFXP20_12 result;

    // Special case when x == 0.
    if (x == LW_TYPES_FXP_ZERO)
    {
        return LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1);
    }

    // Callwlate via Horner Scheme.
    result = LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1);
    for (i = EXP_TAYLOR_SERIES_NUM_STEPS; i > 0; i--)
    {
        result = multUFXP20_12(result, x / i);
        result += LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1);
    }

    return result;
}

/*!
 * Callwlates natural logarithm function f(x) = ln(x) for FXP value with
 * requirement 1 <= x <= 2.  Reduces range before approximating via the Gregory
 * Series.
 *
 * Reduce range of input to that which has an acceptable amount of error.  In
 * this case, reducing range from [1,2] -> [2^-0.2, 2^0.2].  This is
 * accomplished by implementing the following segmentations of the input range:
 *
 * [1, 2^0.2) -unity -> [1, 2^0.2)
 * [2^0.2, 2^0.6) -/2^0.4-> [2^-0.2, 2^0.2)
 * [2^0.6, 2^1] -/2^0.8-> [2^-0.2, 2^0.2]
 *
 * @param[in] x
 *     Value for which to find natural logarithm.  @note x must be in range
 *     [1,2].
 *
 * @return Callwlation of ln(x).
 */
static LwSFXP20_12
s_lnFXPGregory
(
    LwUFXP20_12 x
)
{
    LwSFXP20_12 correction = LW_TYPES_FXP_ZERO;

    // Range reduction with 5 segments.
    if (x >= NUM_2_POW_0P6_UFXP20_12)
    {
        x = multUFXP20_12(x, NUM_2_POW_M0P8_UFXP20_12);
        correction = 2 * LN_2_POW_M0P4_SFXP20_12;
    }
    else if (x >= NUM_2_POW_0P2_UFXP20_12)
    {
        x = multUFXP20_12(x, NUM_2_POW_M0P4_UFXP20_12);
        correction = LN_2_POW_M0P4_SFXP20_12;
    }

    return s_lnFXPGregoryPoly(x) + correction;
}

/*!
 * Callwlates natural logarithm function f(x) = ln(x) for FXP value with
 * requirement 2^-0.2 <= x <= 2^0.2.  Uses 5th order evaluation of continued
 * fraction of the Gregory Series.
 *
 * The Gregory Series is as follows:
 *
 * ln(x) = ln((1 + z)/(1 - z)) =
 *     2 * ( z + z^3 + z^5 + z^7 + z^9 + ... )
 *               ---   ---   ---   ---
 *                3     5     7     9
 *
 *      Where z = (x - 1)/(x + 1)
 *
 * The 5-th order continued fraction of this series can be resolved to rational
 * polynomial of the form:
 *
 *
 * ln(x) = 2z * ( a + b * z^2 )
 *         ------------------
 *              1 + c * z^2
 *
 *     Where a = 1
 *           b = -4/15 = -0.266667
 *           c = -9/15 = -0.6
 *
 * http://en.wikipedia.org/wiki/Natural_logarithm#Numerical_value
 *
 * @param[in] x
 *     Value for which to find natural logarithm.  @note x must be in range
 *     [2^-0.2,2^0.2].
 *
 * @return Callwlation of ln(x).
 */
static LwSFXP20_12
s_lnFXPGregoryPoly
(
    LwUFXP20_12 x
)
{
    LwSFXP20_12 result;
    LwSFXP20_12 z;
    LwSFXP20_12 z2;

    // z = (x - 1) / (x + 1)
    z = (LwSFXP20_12)x;
    z = ((z - LW_TYPES_S32_TO_SFXP_X_Y(20, 12, 1)) << 12) /
            (z + LW_TYPES_S32_TO_SFXP_X_Y(20, 12, 1));
    z2 = multSFXP20_12(z, z);

    //
    // ln(x) = 2z * ( a + b * z^2 )
    //         ------------------
    //              1 + c * z^2
    //
    result = multSFXP20_12((2 * z), (LN_GREGORY_A_SFXP20_12 + multSFXP20_12(LN_GREGORY_B_SFXP20_12, z2)));
    result = (result << 12) / ( LW_TYPES_S32_TO_SFXP_X_Y(20, 12, 1) + multSFXP20_12(LN_GREGORY_C_SFXP20_12, z2));

    return result;
}
