/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lib_fxp_falcon.c
 * @brief  Library of FXP math functions (Falcon implementation)
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
#define LW_U64_BIT_SIZE         (8 * sizeof(LwU64))
#define LW_U32_BIT_SIZE         (8 * sizeof(LwU32))
#define LW_U16_BIT_SIZE         (8 * sizeof(LwU16))

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Static Function Prototypes --------------------- */
/* ------------------------- Static Inline Functions --------------------------*/
GCC_ATTRIB_ALWAYSINLINE()
static inline void s_multUFXP(LwU32 *pResult, LwU16 *pX, LwU16 *pY, LwU32 nChunks, LwU32 shift);

/*!
 * @brief   Helper function to multiply two unsigned 32-bit FXP numbers,
 *          keeping all of the operations within 32-bits, producing an output
 *          number which has been right-shifted by the specified number of bits.
 *
 * This operation discards (with rounding) the specified number of fractional
 * bits from the result and checks for an overflow in the integer bits, while
 * keeping all math within 32-bits. If the caller has requested a failure on
 * an overflow, we force a failure, otherwise we return LW_U32_MAX.
 *
 * @param[in]   nShiftBits      Number of bits by which to right shift the result.
 *                              Expected to be in the range [0, 64).
 * @param[in]   x               First 32-bit operand.
 * @param[in]   y               Second 32-bit operand.
 * @param[in]   bFailOnOverflow Boolean denoting whether we should fail on a
 *                              32-bit overflow.
 *
 * @return  (x * y) >> nShiftBits   On success  (without overflow)
 * @return  LW_U32_MAX              On overflow (if @ref bFailOnOverflow ==
 *                                                           LW_FALSE)
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwUFXP32
s_multUFXP32Helper
(
    LwU8     nShiftBits,
    LwUFXP32 x,
    LwUFXP32 y,
    LwBool   bFailOnOverflow
)
{
    //<! z0 stores the lower 32-bits of the product (bits 31..0).
    LwU32 z0;
    //<! z1 stores the upper 32-bits of the product (bits 63..32).
    LwU32 z1 = 0;
    //<! z2 stores the possible rounding overflow into the bit 64.
    LwU32 z2 = 0;
    LwU32 tmp;
    LwU32 tmp1;

    //
    // 0. Validate input
    //
    // Shift by more than 63 bits will remove all significant bits of result.
    // It's an unexpected/supported usecase => throw a breakpoint and return 0.
    //
    if (nShiftBits >= (2 * LW_U32_BIT_SIZE))
    {
        PMU_BREAKPOINT();
        return 0;
    }

    //
    // 1. Compute full product:
    //
    // Break down each number as a sum of two 16-bit parts:
    //     x = (a << 16) + b
    //     y = (c << 16) + d
    //
    // This allows to break up the multiplication into four parts:
    //     x * y = (a * c) << 32 + (a * d + b * c) << 16 + (b * d)
    //

    // Compute bits 31:0 (b * d).
    z0 = LwU32_LO16(x) * LwU32_LO16(y);

    // Compute bits 47:16 (a * d + b * c).
    tmp  = LwU32_HI16(x) * LwU32_LO16(y);
    tmp1 = LwU32_LO16(x) * LwU32_HI16(y);
    tmp += tmp1;
    if (tmp < tmp1)
    {
        // Set bit 48 (as an overflow).
        z1 += LWBIT32(16);
    }
    z1 += LwU32_HI16(tmp);
    tmp = LwU32_LO16(tmp) << 16;
    z0 += tmp;
    if (z0 < tmp)
    {
        z1++;
    }

    // Compute bits 63:32 (a * c).
    z1 += LwU32_HI16(x) * LwU32_HI16(y);

    //
    // By this point we had no overflow to @ref z2 since maximum possible
    // product is: 0xFFFF_FFFF * 0xFFFF_FFFF = 0xFFFF_FFFE_0000_0001
    //
    // Note that overflow to @ref z2 is possible only if rounding in subsequent
    // step will impact bits 33..63 (when @ref nShiftBits is 34 or more). This
    // is used in subsequent steps to avoid an unnecessary handling of @ref z2
    // when it must be zero.
    //

    // 2. Round result before loosing LSBs
    tmp = LWBIT32((nShiftBits - 1) % LW_U32_BIT_SIZE);
    if (nShiftBits > LW_U32_BIT_SIZE)
    {
        z1 += tmp;
        if (z1 < tmp)
        {
            z2++;
        }
    }
    else if (nShiftBits > 0)
    {
        z0 += tmp;
        if (z0 < tmp)
        {
            z1++;
            // This cannot overflow to z2.
        }
    }

    // 3. Adjust result to desired precision
    if (nShiftBits >= LW_U32_BIT_SIZE)
    {
        z0 = z1;
        z1 = z2;
        nShiftBits -= LW_U32_BIT_SIZE;
    }
    // By this point @ref z2 must be zero so ignoring in further math.
    z0 = (z0 >> nShiftBits) + (z1 << (LW_U32_BIT_SIZE - nShiftBits));
    z1 = (z1 >> nShiftBits);

    // 4. Check for an overflow
    if (z1 != 0)
    {
        z0 = LW_U32_MAX;

        // Fail on overflow if the caller has requested the same.
        if (bFailOnOverflow)
        {
            PMU_BREAKPOINT();
        }
    }

    return (LwUFXP32)z0;
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc MultUFXP32
 */
LwUFXP32
multUFXP32
(
    LwU8     nShiftBits,
    LwUFXP32 x,
    LwUFXP32 y
)
{
    return s_multUFXP32Helper(nShiftBits, x, y, LW_TRUE);
}

/*!
 * @copydoc MultUFXP32
 */
LwUFXP32
multUFXP32FailSafe
(
    LwU8     nShiftBits,
    LwUFXP32 x,
    LwUFXP32 y
)
{
    return s_multUFXP32Helper(nShiftBits, x, y, LW_FALSE);
}

/*!
 * @copydoc MultUFXP64
 */
LwU64
multUFXP64
(
    LwU8  nShiftBits,
    LwU64 x,
    LwU64 y
)
{
    LWU64_TYPE xc;
    LWU64_TYPE yc;
    //
    // Over-allocating memory (by 2x) for the result to avoid handling
    // corner cases related to carry and shifting. 64 * 64 bit product
    // yields 128 bit result so we are allocating 256 bits.
    //
    LwU32 result[2 * LWU64_TYPE_PARTS_ARR_16_MAX];
    LwU32 overflow = 0;
    LwU32 index;

    xc.val = x;
    yc.val = y;

    //
    // 0. Validate input
    //
    // Shift by more than 127 bits will remove all significant bits of result.
    // It's an unexpected/supported usecase => throw a breakpoint and return 0.
    //
    if (nShiftBits >= (2 * LW_U64_BIT_SIZE))
    {
        PMU_BREAKPOINT();
        return 0;
    }

    // Compute 128 bit product and right-shift by the requested number of bits.
    s_multUFXP(result, xc.partsArr16, yc.partsArr16,
               LWU64_TYPE_PARTS_ARR_16_MAX, nShiftBits);

    // Detect an overlfow or simply extract the result.
    for (index = LWU64_TYPE_PARTS_ARR_32_MAX;
         index < (2 * LWU64_TYPE_PARTS_ARR_32_MAX);
         index++)
    {
        overflow |= result[index];
    }
    if (overflow != 0)
    {
        PMU_BREAKPOINT();
        return LW_U64_MAX;
    }
    else
    {
        LWU64_TYPE z;

        z.parts.hi = result[1];
        z.parts.lo = result[0];

        return z.val;
    }
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Helper performing N*16 bit multiplication and result adjustment.
 *
 * @param[out]  pResult     Caller allocated buffer to hold the result of the
 *                          multiplication as well as temporary computations.
 *                          Must be twice the size of the expected result.
 * @param[in]   pX          First operand as an array of 16 bit integers.
 * @param[in]   pY          Second operand as an array of 16 bit integers.
 * @param[in]   nChunks     Size of the operands in units of 16 bit integers.
 *                          Must be power of 2.
 * @param[in]   shift       Number of bits the result should be right-shifted.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
s_multUFXP
(
    LwU32 *pResult,
    LwU16 *pX,
    LwU16 *pY,
    LwU32  nChunks,
    LwU32  shift
)
{
    LwU32 xIndex;
    LwU32 yIndex;
    LwU32 index;
    LwU32 carry;

    // Zero out the temporary (computation) / result buffer.
    for (index = 0; index < (2 * nChunks); index++)
    {
        pResult[index] = 0;
    }

    // Compute full product and store in temporary buffer (expanded).
    for (xIndex = 0; xIndex < nChunks; xIndex++)
    {
        for (yIndex = 0; yIndex < nChunks; yIndex++)
        {
            LwU32 x = (LwU32)pX[xIndex];
            LwU32 y = (LwU32)pY[yIndex];
            LwU32 z = x * y;

            pResult[xIndex + yIndex]     += LwU32_LO16(z);
            pResult[xIndex + yIndex + 1] += LwU32_HI16(z);
        }
    }

    // Apply rounding before bits are lost due to right-shifting.
    if (shift > 0)
    {
        LwU32 idx = (shift - 1) / LW_U16_BIT_SIZE;
        LwU32 bit = (shift - 1) % LW_U16_BIT_SIZE;

        pResult[idx] += LWBIT32(bit);
    }

    // Account for overflows between individual 16 bit parts of the result.
    carry = 0;
    for (index = 0; index < (2 * nChunks); index++)
    {
        pResult[index] += carry;
        carry           = LwU32_HI16(pResult[index]);
        pResult[index]  = LwU32_LO16(pResult[index]);
    }
    // if (carry == 1) => result has one more bit due to rounding overflow.

    // Compact the result in 32 bit integers and clear the upper half.
    for (index = 0; index < nChunks; index++)
    {
        pResult[index] = pResult[2 * index] + (pResult[2 * index + 1] << 16);
    }
    // Account for the extra bit (in case of a rounding overflow).
    pResult[index++] = carry;
    while (index < (2 * nChunks))
    {
        pResult[index++] = 0;
    }

    // Adjust the result by right-shifting by the requested amount of bits.
    {
        LwU32 idx = shift / LW_U32_BIT_SIZE;
        LwU32 bit = shift % LW_U32_BIT_SIZE;

        for (index = 0; index < nChunks; index++)
        {
            pResult[index] =
                (pResult[index + idx]     >> bit) +
                (pResult[index + idx + 1] << (LW_U32_BIT_SIZE - bit));
        }
    }
}
