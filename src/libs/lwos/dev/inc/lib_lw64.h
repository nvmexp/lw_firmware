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
 * @file    lib_lw64.h
 * @copydoc lib_lw64.c
 */

#ifndef LIB_LW64_H
#define LIB_LW64_H

/* ------------------------- System Includes -------------------------------- */
#include "string.h"
#include "lwuproc.h"
#include "lwrtosHooks.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- Macros ----------------------------------------- */
/*!
 * @brief   Helper macro hiding error handling differences across use-cases.
 */
#ifdef SAFERTOS
#define lw64Error() lwrtosHookError(lwrtosTaskGetLwrrentTaskHandle(),          \
                                    (LwS32)FLCN_ERR_ILWALID_ARGUMENT)
#else //SAFERTOS
#define lw64Error() OS_HALT()
#endif //SAFERTOS

/*!
 * Number of 16-bit elements in @ref LWU64_TYPE::partsArr16[].
 */
#define LWU64_TYPE_PARTS_ARR_16_MAX             (sizeof(LwU64) / sizeof(LwU16))

/*!
 * Number of 32-bit elements in @ref LWU64_TYPE::partsArr32[].
 */
#define LWU64_TYPE_PARTS_ARR_32_MAX             (sizeof(LwU64) / sizeof(LwU32))

/*
 * @brief Structure for a 64 unsigned value and its lower/higher 32 bit values
 */
typedef union
{
    LwU64 val;
    struct
    {
        LwU32 lo;
        LwU32 hi;
    } parts;
    LwU16 partsArr16[LWU64_TYPE_PARTS_ARR_16_MAX];
    LwU32 partsArr32[LWU64_TYPE_PARTS_ARR_32_MAX];
} LWU64_TYPE;

/*!
 * @brief Macro to get the lower 32 bit portion of a 64 bit unsigned number.
 *
 * @param[in] val64 The LwU64 operand
 *
 * @return The lower 32 bit portion of the operand
 */
#define LWU64_LO(val64)   (val64).parts.lo

/*!
 * @brief Macro to get the upper 32 bit portion of a 64 bit unsigned number.
 *
 * @param[in] val64 The LwU64 operand
 *
 * @return The upper 32 bit portion of the operand
 */
#define LWU64_HI(val64)   (val64).parts.hi

/*!
 * @brief   List of an overlay descriptors required for a 64-bit integer math
 *          support.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM_LS after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#define OSTASK_OVL_DESC_DEFINE_LIB_LW64                                        \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64)                             \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64Umul)

/*!
 * @brief   List of an overlay descriptors required for a 64-bit integer math
 *          support.
 *
 * @note    Use this list of overlays for temporary detachment usecases.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM_LS after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#define OSTASK_OVL_DESC_DEFINE_LIB_LW64_DETACH                                 \
    OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libLw64)                             \
    OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libLw64Umul)

/*!
 * @brief   Macro introduced to enhance code readability: lw64CmpEQ(_pSrc1,_pSrc2) is equivalent to (*_pSrc1 == *pSrc2)
 *
 * @param[in] _pSrc1 pointer to the first LwU64 operand
 * @param[in] _pSrc1 pointer to the first LwU64 operand
 *
 * @return LW_TRUE  if not equal
 * @return LW_FALSE otherwise
 */
#ifdef UPROC_RISCV
#define lw64CmpEQ(_pSrc1, _pSrc2)  ( lw64CmpEQ_INLINE((_pSrc1),(_pSrc2)))
#else // UPROC_RISCV
#define lw64CmpEQ(_pSrc1, _pSrc2)  ( lw64CmpEQ_FUNC((_pSrc1),(_pSrc2)))
#endif // UPROC_RISCV

/*!
 * @brief   Macro introduced to enhance code readability: lw64CmpNE(_pSrc1,_pSrc2) is equivalent to (*_pSrc1 != *pSrc2)
 *
 * @param[in] _pSrc1 pointer to the first LwU64 operand
 * @param[in] _pSrc1 pointer to the first LwU64 operand
 *
 * @return LW_TRUE  if not equal
 * @return LW_FALSE otherwise
 */
#define lw64CmpNE(_pSrc1, _pSrc2)  (!lw64CmpEQ((_pSrc1),(_pSrc2)))

/*!
 * @brief   Macro introduced to enhance code readability: lwU64CmpGT(_pSrc1,_pSrc2) is equivalent to (*_pSrc1 >  *pSrc2)
 *
 * @param[in] _pSrc1 pointer to the first LwU64 operand
 * @param[in] _pSrc1 pointer to the second LwU64 operand
 *
 * @return LW_TRUE  if greater than
 * @return LW_FALSE otherwise
 */
#ifdef UPROC_RISCV
#define lwU64CmpGT(_pSrc1, _pSrc2)  ( lwU64Cmp_INLINE((_pSrc1),(_pSrc2)))
#else // UPROC_RISCV
#define lwU64CmpGT(_pSrc1, _pSrc2)  ( lwU64Cmp_FUNC((_pSrc1),(_pSrc2)))
#endif // UPROC_RISCV

/*!
 * @brief   Macro introduced to enhance code readability: lwU64CmpLE(_pSrc1,_pSrc2) is equivalent to (*_pSrc1 <= *pSrc2)
 *
 * @param[in] _pSrc1 pointer to the first LwU64 operand
 * @param[in] _pSrc1 pointer to the second LwU64 operand
 *
 * @return LW_TRUE  if less than or equal
 * @return LW_FALSE otherwise
 */
#define lwU64CmpLE(_pSrc1, _pSrc2)  (!lwU64CmpGT((_pSrc1),(_pSrc2)))

/*!
 * @brief   Macro introduced to enhance code readability: lwU64CmpLT(_pSrc1,_pSrc2) is equivalent to (*_pSrc1 <  *pSrc2)
 *
 * @param[in] _pSrc1 pointer to the first LwU64 operand
 * @param[in] _pSrc1 pointer to the second LwU64 operand
 *
 * @return LW_TRUE  if less than
 * @return LW_FALSE otherwise
 */
#define lwU64CmpLT(_pSrc1, _pSrc2)  ( lwU64CmpGT((_pSrc2),(_pSrc1)))

/*!
 * @brief   Macro introduced to enhance code readability: lwU64CmpGE(_pSrc1,_pSrc2) is equivalent to (*_pSrc1 >= *pSrc2)
 *
 * @param[in] _pSrc1 pointer to the first LwU64 operand
 * @param[in] _pSrc1 pointer to the second LwU64 operand
 *
 * @return LW_TRUE  if greater than or equal
 * @return LW_FALSE otherwise
 */
#define lwU64CmpGE(_pSrc1, _pSrc2)  (!lwU64CmpGT((_pSrc2),(_pSrc1)))

/*!
 * @brief Helper macro to pack/unpack a RM_FLCN_U64 <-> LwU64.
 *
 * @param[in] pDst Pointer to a destination RM_FLCN_U64 or LwU64
 * @param[in] pSrc Pointer to a source RM_FLCN_U64 or LwU64
 *
 * @return the same pointer which was passed in to pDst.
 *
 * @note Unforutnately, compiler doesn't like casting pointers, so this instead uses memcpy().
 */
#define LW64_FLCN_U64_PACK(pDst, pSrc)                                         \
    memcpy((pDst), (pSrc), sizeof(RM_FLCN_U64))

#ifdef UPROC_RISCV

# define lw64Add        lw64Add_INLINE
# define lw64Add32      lw64Add32_INLINE
# define lw64Sub        lw64Sub_INLINE
# define lwU64Mul       lwU64Mul_INLINE
#define lwU64Div        lwU64Div_INLINE
#define lwU64DivRounded lwU64DivRounded_FUNC
#define lw64Lsl         lw64Lsl_INLINE
#define lw64Lsr         lw64Lsr_INLINE

#else // UPROC_RISCV

#if defined(UINT_OVERFLOW_CHECK)
# define lw64Add        lw64Add_SAFE
# define lw64Add32      lw64Add32_SAFE
# define lw64Sub        lw64Sub_SAFE
# define lwU64Mul       lwU64Mul_SAFE
#else /* defined(UINT_OVERFLOW_CHECK) */
# define lw64Add        lw64Add_MOD
# define lw64Add32      lw64Add32_MOD
# define lw64Sub        lw64Sub_MOD
# define lwU64Mul       lwU64Mul_MOD
#endif /* defined(UINT_OVERFLOW_CHECK) */

#define lwU64Div        lwU64Div_FUNC
#define lwU64DivRounded lwU64DivRounded_FUNC
#define lw64Lsl         lw64Lsl_FUNC
#define lw64Lsr         lw64Lsr_FUNC

#endif // UPROC_RISCV

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
LwBool  lw64CmpEQ_FUNC(LwU64 *pSrc1, LwU64 *pSrc2)
    GCC_ATTRIB_SECTION("imem_resident", "lw64CmpEQ_FUNC");

LwBool  lwU64Cmp_FUNC(LwU64 *pSrc1, LwU64 *pSrc2)
    GCC_ATTRIB_SECTION("imem_resident", "lwU64Cmp_FUNC");


#if defined(UINT_OVERFLOW_CHECK)
void    lw64Add_SAFE(LwU64 *pDst, LwU64 *pSrc1, LwU64 *pSrc2)
    GCC_ATTRIB_SECTION("imem_resident", "lw64Add_SAFE");

void    lw64Add32_SAFE(LwU64 *pOp64, LwU32 val32)
    GCC_ATTRIB_SECTION("imem_resident", "lw64Add32_SAFE");

void    lw64Sub_SAFE(LwU64 *pDst, LwU64 *pSrc1, LwU64 *pSrc2)
    GCC_ATTRIB_SECTION("imem_resident", "lw64Sub_SAFE");

void    lwU64Mul_SAFE(LwU64 *pDst, LwU64 *pSrc1, LwU64 *pSrc2)
    GCC_ATTRIB_SECTION("imem_libLw64", "lwU64Mul_SAFE");
#endif /* defined(UINT_OVERFLOW_CHECK) */


void    lw64Add_MOD(LwU64 *pDst, LwU64 *pSrc1, LwU64 *pSrc2)
    GCC_ATTRIB_SECTION("imem_resident", "lw64Add_MOD");

void    lw64Add32_MOD(LwU64 *pOp64, LwU32 val32)
    GCC_ATTRIB_SECTION("imem_resident", "lw64Add32_MOD");

void    lw64Sub_MOD(LwU64 *pDst, LwU64 *pSrc1, LwU64 *pSrc2)
    GCC_ATTRIB_SECTION("imem_resident", "lw64Sub_MOD");

void    lwU64Mul_MOD(LwU64 *pDst, LwU64 *pSrc1, LwU64 *pSrc2)
    GCC_ATTRIB_SECTION("imem_libLw64", "lwU64Mul_MOD");


void    lwU64Div_FUNC(LwU64 *pDst, LwU64 *pSrc1, LwU64 *pSrc2)
    GCC_ATTRIB_SECTION("imem_libLw64", "lwU64Div_FUNC");

void    lwU64DivRounded_FUNC(LwU64 *pDst, LwU64 *pSrc1, LwU64 *pSrc2)
    GCC_ATTRIB_SECTION("imem_libLw64", "lwU64DivRounded_FUNC");

void    lw64Lsl_FUNC(LwU64 *pDst, LwU64 *pSrc, LwU8 offset)
    GCC_ATTRIB_SECTION("imem_libLw64", "lw64Lsl_FUNC");

void    lw64Lsr_FUNC(LwU64 *pDst, LwU64 *pSrc, LwU8 offset)
    GCC_ATTRIB_SECTION("imem_libLw64", "lw64Lsr_FUNC");

/* ------------------------- Inline Functions ------------------------------- */
/*!
 * @brief   Compares if two operands are equal (src1 == src2)
 *
 * @param[in]   pSrc1   First operand pointer
 * @param[in]   pSrc2   Second operand pointer
 *
 * @return LW_TRUE  if operands are equal
 * @return LW_FALSE otherwise
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwBool
lw64CmpEQ_INLINE
(
    LwU64  *pSrc1,
    LwU64  *pSrc2
)
{
    return (*pSrc1 == *pSrc2);
}

/*!
 * @brief   Unsigned compare if first op. is greater than second (src1 > src2)
 *
 * @param[in]   pSrc1   First operand pointer
 * @param[in]   pSrc2   Second operand pointer
 *
 * @return LW_TRUE  if first op. is greater than second
 * @return LW_FALSE otherwise
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwBool
lwU64Cmp_INLINE

(
    LwU64  *pSrc1,
    LwU64  *pSrc2
)
{
    return (*pSrc1 > *pSrc2);
}

/*!
 * @brief   Performs 64-bit addition (dst = src1 + src2).
 *
 * Used for code that requires Modulo behaviour on overflow.
 *
 * @note Caller must explicitly write a comment as to why
 *       modulo behaviour is needed as per CERT-C INT30-C-EX1.
 *
 * @param[out]  pDst    Result pointer
 * @param[in]   pSrc1   First operand pointer
 * @param[in]   pSrc2   Second operand pointer
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
lw64Add_INLINE
(
    LwU64  *pDst,
    LwU64  *pSrc1,
    LwU64  *pSrc2
)
{
    *pDst = *pSrc1 + *pSrc2;
}

/*!
 * @brief   Adds 32-bit value to 64-bit operand (op64 += val32).
 *
 * Used for code that requires Modulo behaviour on overflow.
 *
 * @note Caller must explicitly write a comment as to why
 *       modulo behaviour is needed as per CERT-C INT30-C-EX1.
 *
 * @param[in,out]   pOp64   64-bit operand
 * @param[in]       val32   32-bit value to add
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
lw64Add32_INLINE
(
    LwU64  *pOp64,
    LwU32   val32
)
{
    LwU64 op2 = (LwU64)val32;

    *pOp64 += op2;
}

/*!
 * @brief   Performs 64-bit subtraction (dst = src1 - src2).
 *
 * Used for code that requires Modulo behaviour on overflow.
 *
 * @note Caller must explicitly write a comment as to why
 *       modulo behaviour is needed as per CERT-C INT30-C-EX1.
 *
 * @param[out]  pDst    Result pointer
 * @param[in]   pSrc1   First operand pointer
 * @param[in]   pSrc2   Second operand pointer
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
lw64Sub_INLINE
(
    LwU64  *pDst,
    LwU64  *pSrc1,
    LwU64  *pSrc2
)
{
    *pDst = *pSrc1 - *pSrc2;
}

/*!
 * @brief   Performs unsigned 64-bit multiplication (dst = src1 * src2).
 *
 * Used for code that requires Modulo behaviour on overflow.
 *
 * @note Caller must explicitly write a comment as to why
 *       modulo behaviour is needed as per CERT-C INT30-C-EX1.
 *
 * @param[out]  pDst    Result pointer
 * @param[in]   pSrc1   First operand pointer
 * @param[in]   pSrc2   Second operand pointer
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
lwU64Mul_INLINE
(
    LwU64  *pDst,
    LwU64  *pSrc1,
    LwU64  *pSrc2
)
{
    *pDst = *pSrc1 * *pSrc2;
}

/*!
 * @brief   Performs unsigned 64-bit division (dst = src1 / src2)
 *
 * @param[out]  pDst    Result pointer
 * @param[in]   pSrc1   First operand pointer
 * @param[in]   pSrc2   Second operand pointer
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
lwU64Div_INLINE
(
    LwU64  *pDst,
    LwU64  *pSrc1,
    LwU64  *pSrc2
)
{
    if (*pSrc2 == 0U)
    {
        lw64Error();
    }
    else
    {
        *pDst = *pSrc1 / *pSrc2;
    }
}

/*!
 * @brief   Performs 64-bit logical left-shift (dst = src << n)
 *
 * @param[out]  pDst    Result pointer
 * @param[in]   pSrc    Operand pointer
 * @param[in]   offset  Number of bits to shift
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
lw64Lsl_INLINE
(
    LwU64  *pDst,
    LwU64  *pSrc,
    LwU8    offset
)
{
    *pDst = *pSrc << offset;
}

/*!
 * @brief   Performs 64-bit logical right-shift (dst = src >> n)
 *
 * @param[out]  pDst    Result pointer
 * @param[in]   pSrc    Operand pointer
 * @param[in]   offset  Number of bits to shift
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
lw64Lsr_INLINE
(
    LwU64  *pDst,
    LwU64  *pSrc,
    LwU8    offset
)
{
    *pDst = *pSrc >> offset;
}

#endif // LIB_LW64_H

