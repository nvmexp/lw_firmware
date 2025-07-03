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
 * @file   lib_fxp_riscv.c
 * @brief  Library of FXP math functions (RISCV implementation)
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
/* ------------------------- Static Function Prototypes --------------------- */
/* ------------------------- Static Inline Functions --------------------------*/
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
    LwU64  x64 = x;
    LwU64  y64 = y;
    LwU64  z64;
    LwBool bOverflow = LW_FALSE;

    //
    // 0. Validate input
    //
    // Shift by more than 63 bits will remove all significant bits of result.
    // It's an unexpected/supported usecase => throw a breakpoint and return 0.
    //
    if (nShiftBits > 63)
    {
        PMU_BREAKPOINT();
        return 0;
    }

    // 1. Compute full product
    z64 = x64 * y64;

    // 2. Round result before loosing LSBs
    if (nShiftBits > 0)
    {
        z64 += LWBIT64(nShiftBits - 1);

        bOverflow = z64 < LWBIT64(nShiftBits - 1);
    }

    // 3. Adjust result to desired precision
    z64 >>= nShiftBits;
    if (bOverflow)
    {
        z64 += LWBIT64(64 - nShiftBits);
    }

    // 4. Check for an overflow
    if (z64 > LW_U32_MAX)
    {
        z64 = LW_U32_MAX;

        // Fail on overflow if the caller has requested the same.
        if (bFailOnOverflow)
        {
            PMU_BREAKPOINT();
        }
    }

    return LwU64_LO32(z64);
}

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
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
    LwU64  zHi;
    LwU64  zLo;
    LwBool bOverflow = LW_FALSE;

    //
    // 0. Validate input
    //
    // Shift by more than 127 bits will remove all significant bits of result.
    // It's an unexpected/supported usecase => throw a breakpoint and return 0.
    //
    if (nShiftBits > 127)
    {
        PMU_BREAKPOINT();
        return 0;
    }

    //
    // 1. Compute full product
    //
    // Using GCC 128-bit support to avoid inlining assembly code:
    //  __asm__ volatile ("mulhu %0, %1, %2" : "=r"(zHi) :"r"(x), "r"(y));
    //  __asm__ volatile ("mul   %0, %1, %2" : "=r"(zLo) :"r"(x), "r"(y));
    //
    {
        unsigned __int128 z128 = (unsigned __int128)x * y;
        zHi = z128 >> 64;
        zLo = (LwU64)z128;
    }

    // 2. Round result before loosing LSBs
    if (nShiftBits >= 65)
    {
        zHi += LWBIT64(nShiftBits - 65);

        bOverflow = zHi < LWBIT64(nShiftBits - 65);
    }
    else if (nShiftBits > 0)
    {
        zLo += LWBIT64(nShiftBits - 1);
        if (zLo < LWBIT64(nShiftBits - 1))
        {
            //
            // After computing the product in step (1) zHi can never exceed
            // the value of (LW_U64_MAX - 1) so this carry bit cannot trigger
            // further overflow.
            //
            zHi++;
        }
    }

    // 3. Adjust result to desired precision
    if (nShiftBits >= 64)
    {
        zLo = (zHi >> (nShiftBits - 64));
        zHi = 0;

        if (bOverflow)
        {
            zLo += LWBIT64(128 - nShiftBits);
        }
    }
    else
    {
        zLo = (zLo >> nShiftBits) | (zHi << (64 - nShiftBits));
        zHi = (zHi >> nShiftBits);
    }

    // 4. Check for an overflow
    if (zHi != 0)
    {
        zLo = LW_U64_MAX;
        PMU_BREAKPOINT();
    }

    return zLo;
}

/* ------------------------- Private Functions ------------------------------ */
