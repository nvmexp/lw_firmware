/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOARDOBJGRPMASK_H
#define BOARDOBJGRPMASK_H

#include "g_boardobjgrpmask.h"

#ifndef G_BOARDOBJGRPMASK_H
#define G_BOARDOBJGRPMASK_H

/*!
 * @file    boardobjgrpmask.h
 *
 * @brief   Holds PMU definitions of structures and interfaces required for
 *          handling Board Object Group Masks supporting between 1..2047 bits.
 *
 * @note    RM & PMU maintain separate copies of BOARDOBJGRPMASK interfaces and
 *          these must be kept in sync (in case of bug-fixes or enhancements).
 *
 * @note    Do not use these functions directly for VFE_EQU code
 *          or derived classes, instead use the VFE specific wrappers
 *          around these functions, or create VFE wrappers if need be.
 *          This is to facilitate the varying VFE index size
 *          (8-bit versus 16-bit) issue in PMU code.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "ctrl/ctrl2080/ctrl2080boardobj.h"

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Board Object Group Mask super-structure.
 *
 * Used to unify access to all BOARDOBJGRPMASK_E** child classes.
 */
typedef struct
{
    /*!
     * @brief   Number of bits supported by the mask.
     */
    LwBoardObjIdx      bitCount;

    /*!
     * @brief   Number of LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE elements
     *          required to store all @ref bitCount bits.
     */
    LwBoardObjMaskIdx  maskDataCount;

    /*!
     * @brief   Start of the array of LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE
     *          elements representing the bit-mask.
     *
     * @note    Must be the last element of the structure.
     */
    LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE  pData[LW2080_CTRL_BOARDOBJGRP_MASK_ARRAY_START_SIZE];
} BOARDOBJGRPMASK;

/*!
 * @brief   BOARDOBJGRPMASK iterating macro.
 *
 * Iterates over all the bit indexes set in the BOARDOBJGRPMASK.
 *
 * @note    to be used in tandem with @ref BOARDOBJGRPMASK_FOR_EACH_SUPER_END
 *
 * @param[in]   _pMask      BOARDOBJGRPMASK pointer
 * @param[out]  _i          LwBoardObjIdx variable (LValue) to hold iterator
 *                          index
 */
#define BOARDOBJGRPMASK_FOR_EACH_SUPER_BEGIN(_pMask, _i)                       \
do {                                                                           \
    CHECK_SCOPE_BEGIN(BOARDOBJGRPMASK_FOR_EACH_SUPER);                         \
    ct_assert(sizeof(_i) == sizeof(LwBoardObjIdx));                            \
    for ((_i) = 0U; (_i) < (_pMask)->bitCount; (_i)++)                         \
    {                                                                          \
        if (!boardObjGrpMaskBitGet_SUPER((_pMask), (_i)))                      \
        {                                                                      \
            continue;                                                          \
        }
#define BOARDOBJGRPMASK_FOR_EACH_SUPER_END                                     \
    }                                                                          \
    CHECK_SCOPE_END(BOARDOBJGRPMASK_FOR_EACH_SUPER);                           \
} while (LW_FALSE)

/*!
 * @brief   BOARDOBJGRPMASK iterating macro.
 *
 * Iterates over all the bit indexes set in the BOARDOBJGRPMASK.
 *
 * @note    to be used in tandem with @ref BOARDOBJGRPMASK_FOR_EACH_END
 *
 * @param[in]   _pMaskEnn   BOARDOBJGRPMASK_E<XYZ> pointer
 * @param[out]  _i          LwBoardObjIdx variable (LValue) to hold iterator
 *                          index
 */
#define BOARDOBJGRPMASK_FOR_EACH_BEGIN(_pMaskEnn, _i)                          \
    BOARDOBJGRPMASK_FOR_EACH_SUPER_BEGIN(&(_pMaskEnn)->super, (_i))            \
    {                                                                          \
        CHECK_SCOPE_BEGIN(BOARDOBJGRPMASK_FOR_EACH);
#define BOARDOBJGRPMASK_FOR_EACH_END                                           \
        CHECK_SCOPE_END(BOARDOBJGRPMASK_FOR_EACH);                             \
    }                                                                          \
    BOARDOBJGRPMASK_FOR_EACH_SUPER_END

/*!
 * @copydoc boardObjGrpMaskBitGet_SUPER
 */
#define boardObjGrpMaskBitGet(_pMaskEnn, _bitIdx)                              \
    boardObjGrpMaskBitGet_SUPER(&(_pMaskEnn)->super, _bitIdx)

/*!
 * @copydoc boardObjGrpMaskBitSet_SUPER
 */
#define boardObjGrpMaskBitSet(_pMaskEnn, _bitIdx)                              \
    boardObjGrpMaskBitSet_SUPER(&((_pMaskEnn)->super), (_bitIdx))

/*!
 * @copydoc boardObjGrpMaskBitClr_SUPER
 */
#define boardObjGrpMaskBitClr(_pMaskEnn, _bitIdx)                              \
    boardObjGrpMaskBitClr_SUPER(&((_pMaskEnn)->super), (_bitIdx))

/*!
 * @copydoc boardObjGrpMaskBitIlw_SUPER
 */
#define boardObjGrpMaskBitIlw(_pMaskEnn, _bitIdx)                              \
    boardObjGrpMaskBitIlw_SUPER(&((_pMaskEnn)->super), (_bitIdx))

/* ------------------------ Function Prototypes ----------------------------- */
// Basic I/O operations.
mockable FLCN_STATUS boardObjGrpMaskImport_FUNC(BOARDOBJGRPMASK *pMask, LwBoardObjIdx bitSize, LW2080_CTRL_BOARDOBJGRP_MASK *pExtMask);
FLCN_STATUS boardObjGrpMaskExport_FUNC(BOARDOBJGRPMASK *pMask, LwBoardObjIdx bitSize, LW2080_CTRL_BOARDOBJGRP_MASK *pExtMask);

// Operations on all bits of a single mask.
FLCN_STATUS     boardObjGrpMaskClr_FUNC(BOARDOBJGRPMASK *pMask);
FLCN_STATUS     boardObjGrpMaskSet_FUNC(BOARDOBJGRPMASK *pMask);
FLCN_STATUS     boardObjGrpMaskIlw_FUNC(BOARDOBJGRPMASK *pMask);
FLCN_STATUS     boardObjGrpMaskBitRangeSet_FUNC  (BOARDOBJGRPMASK *pMask, LwBoardObjIdx bitIdxFirst, LwBoardObjIdx bitIdxLast);
FLCN_STATUS     boardObjGrpMaskBitRangeClr_FUNC  (BOARDOBJGRPMASK *pMask, LwBoardObjIdx bitIdxFirst, LwBoardObjIdx bitIdxLast);
LwBool          boardObjGrpMaskIsZero_FUNC       (BOARDOBJGRPMASK *pMask);
LwBoardObjIdx   boardObjGrpMaskBitSetCount_FUNC  (BOARDOBJGRPMASK *pMask);
LwBoardObjIdx   boardObjGrpMaskBitIdxLowest_FUNC (BOARDOBJGRPMASK *pMask);
LwBoardObjIdx   boardObjGrpMaskBitIdxHighest_FUNC(BOARDOBJGRPMASK *pMask);

// Operations on a multiple masks.
FLCN_STATUS boardObjGrpMaskAnd_FUNC(BOARDOBJGRPMASK *pDst, BOARDOBJGRPMASK *pOp1, BOARDOBJGRPMASK *pOp2);
FLCN_STATUS boardObjGrpMaskOr_FUNC (BOARDOBJGRPMASK *pDst, BOARDOBJGRPMASK *pOp1, BOARDOBJGRPMASK *pOp2);
FLCN_STATUS boardObjGrpMaskXor_FUNC(BOARDOBJGRPMASK *pDst, BOARDOBJGRPMASK *pOp1, BOARDOBJGRPMASK *pOp2);

// Special interfaces.
         FLCN_STATUS boardObjGrpMaskCopy_FUNC    (BOARDOBJGRPMASK *pDst, BOARDOBJGRPMASK *pSrc);
         FLCN_STATUS boardObjGrpMaskRangeCopy_FUNC(BOARDOBJGRPMASK *pDst, BOARDOBJGRPMASK *pSrc, LwBoardObjIdx bitIdxFirst, LwBoardObjIdx bitIdxLast);
         LwBool      boardObjGrpMaskSizeEQ_FUNC  (BOARDOBJGRPMASK *pOp1, BOARDOBJGRPMASK *pOp2);
mockable LwBool      boardObjGrpMaskIsSubset_FUNC(BOARDOBJGRPMASK *pOp1, BOARDOBJGRPMASK *pOp2);
mockable LwBool      boardObjGrpMaskIsEqual_FUNC(BOARDOBJGRPMASK *pOp1, BOARDOBJGRPMASK *pOp2);

/* ------------------------ Inline Functions  -------------------------------- */
/*!
 * @brief   Initializes the Board Object Group Mask to an empty mask.
 *
 * @param[in]   pMask   BOARDOBJGRPMASK* of mask to initialize.
 * @param[in]   bitSize Value specifying size of the mask in bits.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
boardObjGrpMaskInit_SUPER
(
    BOARDOBJGRPMASK *pMask,
    LwBoardObjIdx    bitSize
)
{
    LwBoardObjMaskIdx dataIndex;
    LwBoardObjMaskIdx dataCount = (LwBoardObjMaskIdx)LW2080_CTRL_BOARDOBJGRP_MASK_DATA_SIZE(bitSize);

    pMask->bitCount      = bitSize;
    pMask->maskDataCount = dataCount;

    for (dataIndex = 0U; dataIndex < dataCount; dataIndex++)
    {
        pMask->pData[dataIndex] = LW2080_CTRL_BOARDOBJGRP_MASK_ELEMENT_MIN;
    }
}

/*!
 * @brief   Retrieve a single bit of the BOARDOBJGRPMASK.
 *
 * @param[in]   _pMask  BOARDOBJGRPMASK* of mask.
 * @param[in]   _bitIdx Index of the target bit within the mask.
 *
 * @return  LW_TRUE     if bit is set.
 * @return  LW_FALSE    if bit is not set or is out of allowable range.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwBool
boardObjGrpMaskBitGet_SUPER
(
    BOARDOBJGRPMASK *pMask,
    LwBoardObjIdx    bitIdx
)
{
    if (bitIdx >= pMask->bitCount)
    {
        PMU_HALT();
        return LW_FALSE;
    }

    return ((pMask->pData[LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_INDEX(bitIdx)] &
                LWBIT_TYPE(LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_OFFSET(bitIdx),
                           LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE)) != 0U);
}

/*!
 * @brief   Set a single bit of the BOARDOBJGRPMASK.
 *
 * @param[in]   pMask   BOARDOBJGRPMASK*
 * @param[in]   bitIdx  Index of the target bit within the mask.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
boardObjGrpMaskBitSet_SUPER
(
    BOARDOBJGRPMASK *pMask,
    LwBoardObjIdx    bitIdx
)
{
    if (bitIdx >= pMask->bitCount)
    {
        PMU_HALT();
        return;
    }

    pMask->pData[LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_INDEX(bitIdx)] |=
        LWBIT_TYPE(LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_OFFSET(bitIdx),
                   LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE);
}

/*!
 * @brief   Clear a single bit of the BOARDOBJGRPMASK.
 *
 * @param[in]   pMask   BOARDOBJGRPMASK*
 * @param[in]   bitIdx  Index of the target bit within the mask.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
boardObjGrpMaskBitClr_SUPER
(
    BOARDOBJGRPMASK *pMask,
    LwBoardObjIdx    bitIdx
)
{
    if (bitIdx >= pMask->bitCount)
    {
        PMU_HALT();
        return;
    }

    pMask->pData[LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_INDEX(bitIdx)] &=
        ~LWBIT_TYPE(LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_OFFSET(bitIdx),
                    LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE);
}

/*!
 * @brief   Toggle a single bit of the BOARDOBJGRPMASK.
 *
 * @param[in]   pMask   BOARDOBJGRPMASK* of mask.
 * @param[in]   bitIdx  Index of the target bit within the mask.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
boardObjGrpMaskBitIlw_SUPER
(
    BOARDOBJGRPMASK *pMask,
    LwBoardObjIdx    bitIdx
)
{
    if (bitIdx >= pMask->bitCount)
    {
        PMU_HALT();
        return;
    }

    pMask->pData[LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_INDEX(bitIdx)] ^=
        LWBIT_TYPE(LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_OFFSET(bitIdx),
                   LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE);
}

/* ------------------------ Macro Function Wrappers ------------------------- */
// Operations on all bits of a single mask.
#define boardObjGrpMaskClr(_pMask)                                             \
        boardObjGrpMaskClr_FUNC(&((_pMask)->super))
#define boardObjGrpMaskSet(_pMask)                                             \
        boardObjGrpMaskSet_FUNC(&((_pMask)->super))
#define boardObjGrpMaskIlw(_pMask)                                             \
        boardObjGrpMaskIlw_FUNC(&((_pMask)->super))
#define boardObjGrpMaskBitRangeSet(_pMask, _bitIdxFirst, _bitIdxLast)          \
        boardObjGrpMaskBitRangeSet_FUNC(&((_pMask)->super), (_bitIdxFirst), (_bitIdxLast))
#define boardObjGrpMaskBitRangeClr(_pMask, _bitIdxFirst, _bitIdxLast)          \
        boardObjGrpMaskBitRangeClr_FUNC(&((_pMask)->super), (_bitIdxFirst), (_bitIdxLast))
#define boardObjGrpMaskIsZero(_pMask)                                          \
        boardObjGrpMaskIsZero_FUNC(&((_pMask)->super))
#define boardObjGrpMaskBitSetCount(_pMask)                                     \
        boardObjGrpMaskBitSetCount_FUNC(&((_pMask)->super))
#define boardObjGrpMaskBitIdxLowest(_pMask)                                    \
        boardObjGrpMaskBitIdxLowest_FUNC(&((_pMask)->super))
#define boardObjGrpMaskBitIdxHighest(_pMask)                                   \
        boardObjGrpMaskBitIdxHighest_FUNC(&((_pMask)->super))

// Operations on a multiple masks.
#define boardObjGrpMaskAnd(_pDst,_pOp1,_pOp2)                                  \
        boardObjGrpMaskAnd_FUNC(&((_pDst)->super), &((_pOp1)->super), &((_pOp2)->super))
#define boardObjGrpMaskOr(_pDst,_pOp1,_pOp2)                                   \
        boardObjGrpMaskOr_FUNC(&((_pDst)->super), &((_pOp1)->super), &((_pOp2)->super))
#define boardObjGrpMaskXor(_pDst,_pOp1,_pOp2)                                  \
        boardObjGrpMaskXor_FUNC(&((_pDst)->super), &((_pOp1)->super), &((_pOp2)->super))

// Special interfaces.
#define boardObjGrpMaskCopy(_pDst,_pSrc)                                       \
        boardObjGrpMaskCopy_FUNC(&((_pDst)->super), &((_pSrc)->super))
#define boardObjGrpMaskRangeCopy(_pDst,_pSrc, _bitIdxFirst, _bitIdxLast)       \
        boardObjGrpMaskRangeCopy_FUNC(&((_pDst)->super), &((_pSrc)->super), (_bitIdxFirst), (_bitIdxLast))
#define boardObjGrpMaskSizeEQ(_pOp1,_pOp2)                                     \
        boardObjGrpMaskSizeEQ_FUNC(&((_pOp1)->super), &((_pOp2)->super))
#define boardObjGrpMaskIsSubset(_pOp1,_pOp2)                                   \
        boardObjGrpMaskIsSubset_FUNC(&((_pOp1)->super), &((_pOp2)->super))
#define boardObjGrpMaskIsEqual(_pOp1,_pOp2)                                    \
        boardObjGrpMaskIsEqual_FUNC(&((_pOp1)->super), &((_pOp2)->super))

/* ------------------------ Include Derived Types --------------------------- */
#include "boardobj/boardobjgrpmask_e32.h"
#include "boardobj/boardobjgrpmask_e255.h"
#include "boardobj/boardobjgrpmask_e512.h"
#include "boardobj/boardobjgrpmask_e1024.h"
#include "boardobj/boardobjgrpmask_e2048.h"

#endif // G_BOARDOBJGRPMASK_H
#endif // BOARDOBJGRPMASK_H
