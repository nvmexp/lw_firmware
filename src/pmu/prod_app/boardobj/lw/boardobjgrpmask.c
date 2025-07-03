/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    boardobjgrpmask.c
 * @copydoc boardobjgrpmask.h
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrpmask.h"
#include "pmu/pmuifboardobj.h"

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Compile-time assert to ensure that @ref BOARDOBJGRPMASK::bitCount
 * size is in a range which can be represented with the current size of
 * @ref BOARDOBJGRPMASK::maskDataCount.
 *
 * With N-bit LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE elements, maskDataCount must
 * be no more than log2(N) bits smaller than bitCount.
 *
 * However, given byte-alignment of types, this would only allow
 * maskDataCount to become 1 byte smaller than bitCount when N >= 256 bits,
 * which is quite unlikely to ever happen.  So, implementing a simple
 * >= check while also ensuring that
 * LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE is < 256 bits.
 */
ct_assert((sizeof(((BOARDOBJGRPMASK *)NULL)->bitCount) <=  sizeof(((BOARDOBJGRPMASK *)NULL)->maskDataCount)) &&
          LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_BIT_SIZE < 256);

/* ------------------------ Function Prototypes ----------------------------- */
static void s_boardObjGrpNormalize(BOARDOBJGRPMASK *pMask);

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   Import an API mask's data into a native mask.
 *
 * @param[in,out]   pMask       Pointer to mask to be imported into.
 * @param[in]       bitSize     Size of mask being imported from.
 * @param[in]       pExtMask    Pointer to API mask being imported from.
 *
 * @return  FLCN_OK                     on success.
 * @return  FLCN_ERR_ILWALID_ARGUMENT   if masks are not of the same size.
 */
FLCN_STATUS
boardObjGrpMaskImport_FUNC_IMPL
(
    BOARDOBJGRPMASK                *pMask,
    LwBoardObjIdx                   bitSize,
    LW2080_CTRL_BOARDOBJGRP_MASK   *pExtMask
)
{
    LwBoardObjMaskIdx mIndex;

    // Make sure that no one attempts to change size of the mask.
    if (pMask->bitCount != bitSize)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    for (mIndex = 0; mIndex < pMask->maskDataCount; mIndex++)
    {
        pMask->pData[mIndex] = pExtMask->pData[mIndex];
    }

    // Discard any trailing garbage from the input.
    s_boardObjGrpNormalize(pMask);

    return FLCN_OK;
}

/*!
 * @brief   Import a native mask's data into an API mask.
 *
 * @param[in]       pMask       Pointer to mask to be exported from.
 * @param[in]       bitSize     Size of mask being exported to.
 * @param[in,out]   pExtMask    Pointer to API mask being exported to.
 *
 * @return  FLCN_OK                     on success.
 * @return  FLCN_ERR_ILWALID_ARGUMENT   if masks are not of the same size.
 */
FLCN_STATUS
boardObjGrpMaskExport_FUNC
(
    BOARDOBJGRPMASK                *pMask,
    LwBoardObjIdx                   bitSize,
    LW2080_CTRL_BOARDOBJGRP_MASK   *pExtMask
)
{
    LwBoardObjMaskIdx mIndex;

    // Make sure that no one attempts to change size of the mask.
    if (pMask->bitCount != bitSize)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    for (mIndex = 0; mIndex < pMask->maskDataCount; mIndex++)
    {
        pExtMask->pData[mIndex] = pMask->pData[mIndex];
    }

    return FLCN_OK;
}

/*!
 * @brief   Clear all bits in a mask.
 *
 * @param[in,out]   pMask   Pointer to mask being modified.
 *
 * @return  FLCN_OK on success.
 */
FLCN_STATUS
boardObjGrpMaskClr_FUNC
(
   BOARDOBJGRPMASK *pMask
)
{
    LwBoardObjMaskIdx mIndex;

    for (mIndex = 0; mIndex < pMask->maskDataCount; mIndex++)
    {
        pMask->pData[mIndex] = LW2080_CTRL_BOARDOBJGRP_MASK_ELEMENT_MIN;
    }

    return FLCN_OK;
}

/*!
 * @brief   Set all bits in a mask.
 *
 * @param[in,out]   pMask   Pointer to mask being modified.
 *
 * @return  FLCN_OK on success.
 */
FLCN_STATUS
boardObjGrpMaskSet_FUNC
(
   BOARDOBJGRPMASK *pMask
)
{
    LwBoardObjMaskIdx mIndex;

    for (mIndex = 0; mIndex < pMask->maskDataCount; mIndex++)
    {
        pMask->pData[mIndex] = LW2080_CTRL_BOARDOBJGRP_MASK_ELEMENT_MAX;
    }

    s_boardObjGrpNormalize(pMask);

    return FLCN_OK;
}

/*!
 * @brief   Set all bits within a particular range for a mask.
 *
 * Set all bits in range [@ref bitIdxFirst, @ref bitIdxLast]
 *
 * @param[in,out]   pMask       Pointer to mask being modified.
 * @param[in]       bitIdxFirst The bit defining beginning of range.
 * @param[in]       bitIdxLast  The bit defining end of range.
 *
 * @return  FLCN_OK                     on success
 * @return  FLCN_ERR_ILWALID_ARGUMENT   if bitIdxFirst > bitIdxLast
 * @return  FLCN_ERR_ILWALID_ARGUMENT   if bitIdxLast > size of mask
 */
FLCN_STATUS
boardObjGrpMaskBitRangeSet_FUNC
(
    BOARDOBJGRPMASK *pMask,
    LwBoardObjIdx    bitIdxFirst,
    LwBoardObjIdx    bitIdxLast
)
{
    LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE mFirst;
    LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE mLast;
    LwBoardObjMaskIdx mIndex;
    LwBoardObjMaskIdx mIndexFirst;
    LwBoardObjMaskIdx mIndexLast;

    if ((bitIdxFirst > bitIdxLast) ||
        (bitIdxLast >= pMask->bitCount))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    mIndexFirst = LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_INDEX(bitIdxFirst);
    mIndexLast  = LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_INDEX(bitIdxLast);

    bitIdxFirst = LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_OFFSET(bitIdxFirst);
    bitIdxLast  = LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_OFFSET(bitIdxLast);

    mFirst = LW2080_CTRL_BOARDOBJGRP_MASK_ELEMENT_MAX << bitIdxFirst;
    mLast  = LW2080_CTRL_BOARDOBJGRP_MASK_ELEMENT_MAX >> (32U - (bitIdxLast + 1U));

    if (mIndexFirst == mIndexLast)
    {
        pMask->pData[mIndexFirst] |= (mFirst & mLast);
    }
    else
    {
        pMask->pData[mIndexFirst] |= mFirst;
        pMask->pData[mIndexLast]  |= mLast;
    }
    for (mIndex = mIndexFirst + 1U; mIndex < mIndexLast; mIndex++)
    {
        pMask->pData[mIndex] = LW2080_CTRL_BOARDOBJGRP_MASK_ELEMENT_MAX;
    }

    return FLCN_OK;
}

/*!
 * @brief   Ilwert all bits in a mask.
 *
 * @param[in,out]   pMask   Pointer to mask being modified.
 *
 * @return  FLCN_OK on success.
 */
FLCN_STATUS
boardObjGrpMaskIlw_FUNC
(
   BOARDOBJGRPMASK *pMask
)
{
    LwBoardObjMaskIdx mIndex;

    for (mIndex = 0; mIndex < pMask->maskDataCount; mIndex++)
    {
        pMask->pData[mIndex] = ~pMask->pData[mIndex];
    }

    s_boardObjGrpNormalize(pMask);

    return FLCN_OK;
}

/*!
 * @brief   Check if mask has all bits unset.
 *
 * @param[in]   pMask   Pointer to mask being checked.
 *
 * @return  LW_TRUE     if all bits are unset.
 * @return  LW_FALSE    if any bit is unset.
 */
LwBool
boardObjGrpMaskIsZero_FUNC
(
   BOARDOBJGRPMASK *pMask
)
{
    LwBoardObjMaskIdx mIndex;

    for (mIndex = 0; mIndex < pMask->maskDataCount; mIndex++)
    {
        if (pMask->pData[mIndex] != LW2080_CTRL_BOARDOBJGRP_MASK_ELEMENT_MIN)
        {
            return LW_FALSE;
        }
    }

    return LW_TRUE;
}

/*!
 * @brief   Get number of set bits in a mask.
 *
 * @todo    Remove dependence on NUMSETBITS_32, make generic to @ref
 *          LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_BIT_SIZE
 *
 * @param[in]   pMask   Pointer to mask being checked.
 *
 * @return  Number of set bits.
 */
LwBoardObjIdx
boardObjGrpMaskBitSetCount_FUNC
(
   BOARDOBJGRPMASK *pMask
)
{
    LwBoardObjMaskIdx mIndex;
    LwBoardObjIdx     result = 0;

    for (mIndex = 0; mIndex < pMask->maskDataCount; mIndex++)
    {
        LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE m = pMask->pData[mIndex];

        NUMSETBITS_32(m);

        result += (LwBoardObjIdx)m;
    }

    return result;
}

/*!
 * @brief   Get index of lowest set bit in mask. I.e. lowest significant bit
 *          set.
 *
 * @todo    Remove dependence on LOWESTBITIDX_32, make generic to @ref
 *          LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_BIT_SIZE
 *
 * @param[in]   pMask   Pointer to mask being checked.
 *
 * @return  Lowest significant bit set.
 */
LwBoardObjIdx
boardObjGrpMaskBitIdxLowest_FUNC
(
   BOARDOBJGRPMASK *pMask
)
{
    LwBoardObjMaskIdx mIndex;
    LwBoardObjIdx     result = LW2080_CTRL_BOARDOBJ_IDX_ILWALID;

    for (mIndex = 0; mIndex < pMask->maskDataCount; mIndex++)
    {
        LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE m = pMask->pData[mIndex];

        if (m != 0U)
        {
            LOWESTBITIDX_32(m);

            result = (LwBoardObjIdx)m +
                mIndex * LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_BIT_SIZE;

            break;
        }
    }

    return result;
}

/*!
 * @brief   Get index of highest set bit in mask. I.e. Most significant bit set.
 *
 * @todo    Remove dependence on HIGHESTBITIDX_32, make generic to @ref
 *          LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_BIT_SIZE
 *
 * @param[in]   pMask   Pointer to mask being checked.
 *
 * @return  Most significant bit set.
 */
LwBoardObjIdx
boardObjGrpMaskBitIdxHighest_FUNC
(
   BOARDOBJGRPMASK *pMask
)
{
    LwBoardObjMaskIdx mIndex;
    LwBoardObjIdx     result = LW2080_CTRL_BOARDOBJ_IDX_ILWALID;

    for (mIndex = pMask->maskDataCount; mIndex > 0U; )
    {
        LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE m = pMask->pData[--mIndex];

        if (m != 0U)
        {
            HIGHESTBITIDX_32(m);

            result = (LwBoardObjIdx)m +
                mIndex * LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_BIT_SIZE;

            break;
        }
    }

    return result;
}

/*!
 * @brief   Bitwise AND two masks together.
 *
 * @param[in,out]   pDst    Pointer to destination mask.
 * @param[in]       pOp1    Pointer to first source mask.
 * @param[in]       pOp2    Pointer to second source mask.
 *
 * @return  FLCN_OK                     on success.
 * @return  FLCN_ERR_ILWALID_ARGUMENT   if masks are not of equal size.
 */
FLCN_STATUS
boardObjGrpMaskAnd_FUNC
(
   BOARDOBJGRPMASK *pDst,
   BOARDOBJGRPMASK *pOp1,
   BOARDOBJGRPMASK *pOp2
)
{
    LwBoardObjMaskIdx mIndex;

    if ((!boardObjGrpMaskSizeEQ_FUNC(pDst, pOp1)) ||
        (!boardObjGrpMaskSizeEQ_FUNC(pDst, pOp2)))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    for (mIndex = 0; mIndex < pDst->maskDataCount; mIndex++)
    {
        pDst->pData[mIndex] = pOp1->pData[mIndex] & pOp2->pData[mIndex];
    }

    return FLCN_OK;
}

/*!
 * @brief   Bitwise OR two masks together.
 *
 * @param[in,out]   pDst    Pointer to destination mask.
 * @param[in]       pOp1    Pointer to first source mask.
 * @param[in]       pOp2    Pointer to second source mask.
 *
 * @return  FLCN_OK                     on success.
 * @return  FLCN_ERR_ILWALID_ARGUMENT   if masks are not of equal size.
 */
FLCN_STATUS
boardObjGrpMaskOr_FUNC
(
   BOARDOBJGRPMASK *pDst,
   BOARDOBJGRPMASK *pOp1,
   BOARDOBJGRPMASK *pOp2
)
{
    LwBoardObjMaskIdx mIndex;

    if ((!boardObjGrpMaskSizeEQ_FUNC(pDst, pOp1)) ||
        (!boardObjGrpMaskSizeEQ_FUNC(pDst, pOp2)))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    for (mIndex = 0; mIndex < pDst->maskDataCount; mIndex++)
    {
        pDst->pData[mIndex] = pOp1->pData[mIndex] | pOp2->pData[mIndex];
    }

    return FLCN_OK;
}

/*!
 * @brief   Bitwise XOR two masks together.
 *
 * @param[in,out]   pDst    Pointer to destination mask.
 * @param[in]       pOp1    Pointer to first source mask.
 * @param[in]       pOp2    Pointer to second source mask.
 *
 * @return  FLCN_OK                     on success.
 * @return  FLCN_ERR_ILWALID_ARGUMENT   if masks are not of equal size.
 */
FLCN_STATUS
boardObjGrpMaskXor_FUNC
(
   BOARDOBJGRPMASK *pDst,
   BOARDOBJGRPMASK *pOp1,
   BOARDOBJGRPMASK *pOp2
)
{
    LwBoardObjMaskIdx mIndex;

    if ((!boardObjGrpMaskSizeEQ_FUNC(pDst, pOp1)) ||
        (!boardObjGrpMaskSizeEQ_FUNC(pDst, pOp2)))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    for (mIndex = 0; mIndex < pDst->maskDataCount; mIndex++)
    {
        pDst->pData[mIndex] = pOp1->pData[mIndex] ^ pOp2->pData[mIndex];
    }

    return FLCN_OK;
}

/*!
 * @brief   Copy contents of one mask to another.
 *
 * @param[in,out]   pDst    Pointer to destination mask.
 * @param[in]       pSrc    Pointer to source mask.
 *
 * @return  FLCN_OK                     on success.
 * @return  FLCN_ERR_ILWALID_ARGUMENT   if masks are not of equal size.
 */
FLCN_STATUS
boardObjGrpMaskCopy_FUNC
(
   BOARDOBJGRPMASK *pDst,
   BOARDOBJGRPMASK *pSrc
)
{
    LwBoardObjMaskIdx mIndex;

    if (!boardObjGrpMaskSizeEQ_FUNC(pDst, pSrc))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    for (mIndex = 0; mIndex < pDst->maskDataCount; mIndex++)
    {
        pDst->pData[mIndex] = pSrc->pData[mIndex];
    }

    return FLCN_OK;
}

/*!
 * @brief   Copy range of contents of one mask to another.
 *
 * @param[in,out]   pDst        Pointer to destination mask.
 * @param[in]       pSrc        Pointer to source mask.
 * @param[in]       bitIdxFirst Index from which to begin copying.
 * @param[in]       bitIdxLast  Index at which to stop copying.
 *
 * @return  @ref FLCN_OK                    Success.
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pDst or pSrc are NULL; bitIdxFirst
 *                                          is greater than bitIdxLast;
 *                                          bitIdxLast is outside the range of
 *                                          the masks; or the masks are not of
 *                                          equal size.
 */
FLCN_STATUS
boardObjGrpMaskRangeCopy_FUNC
(
   BOARDOBJGRPMASK *pDst,
   BOARDOBJGRPMASK *pSrc,
   LwBoardObjIdx    bitIdxFirst,
   LwBoardObjIdx    bitIdxLast
)
{
    FLCN_STATUS status;
    const LwBoardObjMaskIdx mBitsPerPrimitive =
        sizeof(LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE) * 8U;
    const LwBoardObjMaskIdx mIndexFirst =
        LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_INDEX(bitIdxFirst);
    const LwBoardObjMaskIdx mBitIndexFirst =
        LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_OFFSET(bitIdxFirst);
    const LwBoardObjMaskIdx mIndexLast =
        LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_INDEX(bitIdxLast);
    const LwBoardObjMaskIdx mBitIndexLast =
        LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_OFFSET(bitIdxLast);
    LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE mFirstOutsideRangeSet;
    LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE mFirstInsideRangeSrc;
    LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE mLastOutsideRangeSet;
    LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE mLastInsideRangeSrc;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDst != NULL) &&
        (pSrc != NULL) &&
        (bitIdxFirst <= bitIdxLast) &&
        (bitIdxLast < pDst->bitCount),
        FLCN_ERR_ILWALID_ARGUMENT,
        boardObjGrpMaskRangeCopy_FUNC_exit);

    PMU_ASSERT_TRUE_OR_GOTO(status,
        boardObjGrpMaskSizeEQ_FUNC(pDst, pSrc),
        FLCN_ERR_ILWALID_ARGUMENT,
        boardObjGrpMaskRangeCopy_FUNC_exit);

    //
    // Need to get two values for both the first and last index:
    //  1.) Mask with all bits outside the range set and no bits inside the
    //      range set - to be ANDed with the dst to clear only the bits in the
    //      range
    //  2.) Mask with all bits outside the range cleared and the bits inside the
    //      range in src set - to be ORed with the dst to set only the bits in
    //      the range
    //
    // Note that shifting by more than the number of bits available is
    // technically undefined, so there are checks here to prevent that.
    //
    mFirstOutsideRangeSet = mBitIndexFirst == 0U ?
        0U :
        (LW2080_CTRL_BOARDOBJGRP_MASK_ELEMENT_MAX >> (mBitsPerPrimitive - mBitIndexFirst));
    mFirstInsideRangeSrc = ~mFirstOutsideRangeSet & pSrc->pData[mIndexFirst];

    mLastOutsideRangeSet = (mBitIndexLast + 1U) == mBitsPerPrimitive ?
        0U :
        (LW2080_CTRL_BOARDOBJGRP_MASK_ELEMENT_MAX << (mBitIndexLast + 1U));
    mLastInsideRangeSrc = ~mLastOutsideRangeSet & pSrc->pData[mIndexLast];

    if (mIndexFirst == mIndexLast)
    {
        //
        // Need to combine the two masks:
        //  1.) OR them together to get all bits outside the range, each of
        //      which has one "end" set, set.
        //  2.) AND them together to get only the bits inside the range, which
        //      are set in both, set.
        //
        const LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE mCombinedOutsideRangeSet =
            (mFirstOutsideRangeSet | mLastOutsideRangeSet);
        const LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE mCombinedInsideRangeSrc =
            (mFirstInsideRangeSrc & mLastInsideRangeSrc);

        pDst->pData[mIndexFirst] &= mCombinedOutsideRangeSet;
        pDst->pData[mIndexFirst] |= mCombinedInsideRangeSrc;
    }
    else
    {
        LwBoardObjMaskIdx mIndex;

        // Set the two endpoints
        pDst->pData[mIndexFirst] &= mFirstOutsideRangeSet;
        pDst->pData[mIndexFirst] |= mFirstInsideRangeSrc;
        pDst->pData[mIndexLast] &= mLastOutsideRangeSet;
        pDst->pData[mIndexLast] |= mLastInsideRangeSrc;

        // Loop through all elements within the endpoints and do direct copy
        for (mIndex = mIndexFirst + 1U; mIndex < mIndexLast; mIndex++)
        {
            pDst->pData[mIndex] = pSrc->pData[mIndex];
        }
    }

boardObjGrpMaskRangeCopy_FUNC_exit:
    return status;
}

/*!
 * @brief   Check if two masks are of equal size.
 *
 * @param[in]   pOp1    Pointer to first mask.
 * @param[in]   pOp2    Pointer to second mask.
 *
 * @return  LW_TRUE     if sizes are equal.
 * @return  LW_FALSE    if sizes are not equal.
 */
LwBool
boardObjGrpMaskSizeEQ_FUNC
(
   BOARDOBJGRPMASK *pOp1,
   BOARDOBJGRPMASK *pOp2
)
{
    return pOp1->bitCount == pOp2->bitCount;
}

/*!
 * @brief   Check if mask pOp1 is a subset of pOp2. I.e. all bits set in pOp1
 *          are set in pOp2.
 *
 * @param[in]   pOp1    Pointer to first mask.
 * @param[in]   pOp2    Pointer to second mask.
 *
 * @return  LW_TRUE     if pOp1 is a subset of pOp2.
 * @return  LW_FALSE    if pOp1 is NOT a subset of pOp2 OR sizes are not equal.
 */
LwBool
boardObjGrpMaskIsSubset_FUNC_IMPL
(
   BOARDOBJGRPMASK *pOp1,
   BOARDOBJGRPMASK *pOp2
)
{
    LwBoardObjMaskIdx mIndex;

    if (!boardObjGrpMaskSizeEQ_FUNC(pOp1, pOp2))
    {
        PMU_BREAKPOINT();
        return LW_FALSE;
    }

    for (mIndex = 0; mIndex < pOp1->maskDataCount; mIndex++)
    {
        LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE op1 = pOp1->pData[mIndex];
        LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE op2 = pOp2->pData[mIndex];

        if ((op1 & op2) != op1)
        {
            return LW_FALSE;
        }
    }

    return LW_TRUE;
}

/*!
 * @brief   Check if all bits set in pOp1 and pOp2 are the same.
 *
 * @param[in]   pOp1    Pointer to first mask.
 * @param[in]   pOp2    Pointer to second mask.
 *
 * @return LW_TRUE  if all bits set are the same.
 * @return LW_FALSE if all bits are not equal or sizes are mismatched.
 */
LwBool
boardObjGrpMaskIsEqual_FUNC_IMPL
(
   BOARDOBJGRPMASK *pOp1,
   BOARDOBJGRPMASK *pOp2
)
{
    LwBoardObjMaskIdx mIndex;

    if (!boardObjGrpMaskSizeEQ_FUNC(pOp1, pOp2))
    {
        PMU_BREAKPOINT();
        return LW_FALSE;
    }

    for (mIndex = 0; mIndex < pOp1->maskDataCount; mIndex++)
    {
        LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE op1 = pOp1->pData[mIndex];
        LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE op2 = pOp2->pData[mIndex];

        if (op1 != op2)
        {
            return LW_FALSE;
        }
    }

    return LW_TRUE;
}

/* ------------------------ Private Static Functions ------------------------ */
/*!
 * @brief   Helper function assuring that unused bits are always zero. Unused
 *          bits are indexes { size .. (maskDataCount * 32 - 1) }.
 *
 * @param[in,out]   pMask   Pointer to mask being modified.
 */
static void
s_boardObjGrpNormalize
(
    BOARDOBJGRPMASK    *pMask
)
{
    LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE bitSize = pMask->bitCount;
    LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE filter;

    bitSize %= LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_BIT_SIZE;

    filter = (bitSize == 0U) ?
        LW2080_CTRL_BOARDOBJGRP_MASK_ELEMENT_MAX : (LWBIT32(bitSize) - 1U);

    pMask->pData[pMask->maskDataCount - 1U] &= filter;
}
