/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lib_lw64.c
 * @brief   Library of LwU64/LwS64 arithmetic/logic operations
 *
 * GCC compiler has to emulate 64-bit math when generating code for 32-bit core,
 * implying expansion of emulated code on each use-case and increasing IMEM use.
 * Therefore most common arithmetic/logic operations were extracted to a library
 * fucntions.
 *
 * Falcon's GCC compiler can optimize function's arguments using three registers
 * however due to 32-bit nature of falcon core we can not efficiently pass three
 * LwU64 arguments so we pass argument pointers instead.
 *
 * @warning All functions were written (as well as new should be added) assuring
 *          that they operate correctly if same argument pointer is passed to it
 *          multiple times.
 *          Example: lw64add(&a,&a,&a) is equivalent to a=a+a (a*=2)
 *
 * @note    Following naming convention was assumed:
 *  - interfaces specific to unsigned operands are prefixed with lwU64*
 *  - interfaces specific to  signed  operands are prefixed with lwS64*
 *  - remaining interfaces common to both      are prefixed with lw64*
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwrtos.h"
#include "flcnretval.h"
#include "lwoslayer.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib_lw64.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Function Prototypes --------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Compares if two operands are equal (src1 == src2)
 *
 * @param[in]   pSrc1   First operand pointer
 * @param[in]   pSrc2   Second operand pointer
 *
 * @return LW_TRUE if operands are equal
 * @return LW_FALSE otherwise
 */
LwBool
lw64CmpEQ_FUNC
(
    LwU64  *pSrc1,
    LwU64  *pSrc2
)
{
    return lw64CmpEQ_INLINE(pSrc1, pSrc2);
}

/*!
 * @brief   Unsigned compare if first op. is greater than second (src1 > src2)
 *
 * @param[in]   pSrc1   First operand pointer
 * @param[in]   pSrc2   Second operand pointer
 *
 * @return LW_TRUE if first op. is greater than second
 * @return LW_FALSE otherwise
 */
LwBool
lwU64Cmp_FUNC
(
    LwU64  *pSrc1,
    LwU64  *pSrc2
)
{
    return lwU64Cmp_INLINE(pSrc1, pSrc2);
}

#if defined(UINT_OVERFLOW_CHECK)
/*!
 * @brief   Performs 64-bit addition (dst = src1 + src2). Checks for overflow and calls lw64Error if it is detected.
 *
 * @param[out]  pDst    Result pointer
 * @param[in]   pSrc1   First operand pointer
 * @param[in]   pSrc2   Second operand pointer
 */
void
lw64Add_SAFE
(
    LwU64  *pDst,
    LwU64  *pSrc1,
    LwU64  *pSrc2
)
{
    if ((LW_U64_MAX - *pSrc1) < *pSrc2)
    {
        // overflow
        lw64Error();
    }
    else
    {
        lw64Add_INLINE(pDst, pSrc1, pSrc2);
    }
}

/*!
 * @brief   Adds 32-bit value to 64-bit operand (op64 += val32).  Checks for overflow and calls lw64Error if it is detected.
 *
 * @param[in,out]   pOp64   64-bit operand
 * @param[in]       val32   32-bit value to add
 */
void
lw64Add32_SAFE
(
    LwU64  *pOp64,
    LwU32   val32
)
{
    LwU64 op2 = (LwU64) val32;
    lw64Add(pOp64, pOp64, &op2);
}

/*!
 * @brief   Performs 64-bit subtraction (dst = src1 - src2). Checks for overflow and calls lw64Error if it is detected.
 *
 * @param[out]  pDst    Result pointer
 * @param[in]   pSrc1   First operand pointer
 * @param[in]   pSrc2   Second operand pointer
 */
void
lw64Sub_SAFE
(
    LwU64  *pDst,
    LwU64  *pSrc1,
    LwU64  *pSrc2
)
{
    if (*pSrc1 < *pSrc2)
    {
        // underflow
        lw64Error();
    }
    else
    {
        lw64Sub_INLINE(pDst, pSrc1, pSrc2);
    }
}

/*!
 * @brief   Performs unsigned 64-bit multiplication (dst = src1 * src2). Checks for overflow and calls lw64Error if it is detected.
 *
 * @param[out]  pDst    Result pointer
 * @param[in]   pSrc1   First operand pointer
 * @param[in]   pSrc2   Second operand pointer
 *
 */
void
lwU64Mul_SAFE
(
    LwU64  *pDst,
    LwU64  *pSrc1,
    LwU64  *pSrc2
)
{
    /* Do a multipilication while testing for overflow. The simple test for overflow is
     * if ((*pSrc1) > (LW_U64_MAX / (*pSrc2)))
     * But division is slow and requires the GCC ___udivdi3 function which inflates our IMEM size.
     * Instead, check the overflow by performing long multiplication using a smaller type
    */
    const LwU8 NUM_BITS_HALF = 32; //Half the number of bits in an LwU64

    LWU64_TYPE lhs;
    LWU64_TYPE rhs;
    lhs.val = *pSrc1;
    rhs.val = *pSrc2;

    //This cannot overflow because the top 32 bits of LWU64_LO(lhs) and LWU64_LO(rhs) are 0
    LwU64 bot_bits = ((LwU64)LWU64_LO(lhs)) * ((LwU64)LWU64_LO(rhs));
    if (LWU64_HI(lhs) == 0U && LWU64_HI(rhs) == 0U)
    {
        *pDst = bot_bits;
        return;
    }
    LwBool overflowed = (LWU64_HI(lhs) != 0U) && (LWU64_HI(rhs) != 0U);
    //This cannot overflow because the top 32 bits of the operands are empty
    LwU64 mid_bits1 = ((LwU64)LWU64_LO(lhs)) * ((LwU64)LWU64_HI(rhs));
    LwU64 mid_bits2 = ((LwU64)LWU64_HI(lhs)) * ((LwU64)LWU64_LO(rhs));

    //If this does overflow, we will catch it in the if statement below and trigger a lw64Error
    LwU64 mid_bits_sum = 0;
    lw64Add(&mid_bits_sum, &mid_bits1, &mid_bits2);
    mid_bits_sum <<= NUM_BITS_HALF;
    lw64Add(pDst, &bot_bits, &mid_bits_sum);
    if (overflowed == LW_TRUE ||
            (mid_bits1 >> NUM_BITS_HALF) != 0U ||
            (mid_bits2 >> NUM_BITS_HALF) != 0U)
    {
        lw64Error();
    }
}
#endif /* defined(UINT_OVERFLOW_CHECK) */

/*!
 * @brief   Performs 64-bit addition (dst = src1 + src2). Used for code that requires Modulo behaviour on overflow.
 *
 * @note Caller must explicitly write a comment as to why modulo behaviour is needed as per CERT-C INT30-C-EX1.
 *
 * @param[out]  pDst    Result pointer
 * @param[in]   pSrc1   First operand pointer
 * @param[in]   pSrc2   Second operand pointer
 */
void
lw64Add_MOD
(
    LwU64  *pDst,
    LwU64  *pSrc1,
    LwU64  *pSrc2
)
{
    lw64Add_INLINE(pDst, pSrc1, pSrc2);
}

/*!
 * @brief   Adds 32-bit value to 64-bit operand (op64 += val32). Used for code that requires Modulo behaviour on overflow.
 *
 * @note Caller must explicitly write a comment as to why modulo behaviour is needed as per CERT-C INT30-C-EX1.
 *
 * @param[in,out]   pOp64   64-bit operand
 * @param[in]       val32   32-bit value to add
 */
void
lw64Add32_MOD
(
    LwU64  *pOp64,
    LwU32   val32
)
{
    LwU64 op2 = (LwU64) val32;
    lw64Add_MOD(pOp64, pOp64, &op2);
}

/*!
 * @brief   Performs 64-bit subtraction (dst = src1 - src2). Used for code that requires Modulo behaviour on overflow.
 *
 * @note Caller must explicitly write a comment as to why modulo behaviour is needed as per CERT-C INT30-C-EX1.
 *
 * @param[out]  pDst    Result pointer
 * @param[in]   pSrc1   First operand pointer
 * @param[in]   pSrc2   Second operand pointer
 */
void
lw64Sub_MOD
(
    LwU64  *pDst,
    LwU64  *pSrc1,
    LwU64  *pSrc2
)
{
    lw64Sub_INLINE(pDst, pSrc1, pSrc2);
}

/*!
 * @brief   Performs unsigned 64-bit multiplication (dst = src1 * src2). Used for code that requires Modulo behaviour on overflow.
 *
 * @note Caller must explicitly write a comment as to why modulo behaviour is needed as per CERT-C INT30-C-EX1.
 *
 * @param[out]  pDst    Result pointer
 * @param[in]   pSrc1   First operand pointer
 * @param[in]   pSrc2   Second operand pointer
 *
 */
void
lwU64Mul_MOD
(
    LwU64  *pDst,
    LwU64  *pSrc1,
    LwU64  *pSrc2
)
{
    lwU64Mul_INLINE(pDst, pSrc1, pSrc2);
}

/*!
 * @brief   Performs unsigned 64-bit division (dst = src1 / src2)
 *
 * @param[out]  pDst    Result pointer
 * @param[in]   pSrc1   First operand pointer
 * @param[in]   pSrc2   Second operand pointer
 *
 */
void
lwU64Div_FUNC
(
    LwU64  *pDst,
    LwU64  *pSrc1,
    LwU64  *pSrc2
)
{
    lwU64Div_INLINE(pDst, pSrc1, pSrc2);
}

/*!
 * @brief   Performs unsigned 64-bit rounded division
 *          (dst = (src1 + (src2 / 2)) / src2)
 *
 * @param[out]  pDst    Result pointer
 * @param[in]   pSrc1   First operand pointer
 * @param[in]   pSrc2   Second operand pointer
 */
void
lwU64DivRounded_FUNC
(
    LwU64  *pDst,
    LwU64  *pSrc1,
    LwU64  *pSrc2
)
{
    LwU64 divisorByTwo;
    LwU64 dividendAdjusted;

    // divisorByTwo = src2/2
    lw64Lsr(&divisorByTwo, pSrc2, 1);

    // dividendAdjusted = src1 + src2/2
    lw64Add(&dividendAdjusted, pSrc1, &divisorByTwo);

    if (lwU64CmpGT(pSrc1, &dividendAdjusted))
    {
        //
        // If there is an overflow in add operation (src1 > src1 + (src2/2)),
        // skip rounded division and do a normal division
        //
        lwU64Div(pDst, pSrc1, pSrc2);
    }
    else
    {
        // dst = (src1 + src2/2) / src2
        lwU64Div(pDst, &dividendAdjusted, pSrc2);
    }
}

/*!
 * @brief   Performs 64-bit logical left-shift (dst = src << n)
 *
 * @param[out]  pDst    Result pointer
 * @param[in]   pSrc    Operand pointer
 * @param[in]   offset  Number of bits to shift
 */
void
lw64Lsl_FUNC
(
    LwU64  *pDst,
    LwU64  *pSrc,
    LwU8    offset
)
{
    lw64Lsl_INLINE(pDst, pSrc, offset);
}

/*!
 * @brief   Performs 64-bit logical right-shift (dst = src >> n)
 *
 * @param[out]  pDst    Result pointer
 * @param[in]   pSrc    Operand pointer
 * @param[in]   offset  Number of bits to shift
 */
void
lw64Lsr_FUNC
(
    LwU64  *pDst,
    LwU64  *pSrc,
    LwU8    offset
)
{
    lw64Lsr_INLINE(pDst, pSrc, offset);
}

/* ------------------------- Private Functions ------------------------------ */
