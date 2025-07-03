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
 * @file    lib_lwf32.h
 * @copydoc lib_lwf32.c
 */

#ifndef LIB_LWF32_H
#define LIB_LWF32_H

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "lwfixedtypes.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros ----------------------------------------- */
/*!
 * @brief   List of an overlay descriptors required by single precision floating
 *          point math (LwF32 support).
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM_LS after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#define OSTASK_OVL_DESC_DEFINE_LIB_LW_F32                                      \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64Umul)                         \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLwF32)

/*!
 * @brief   Macro performing LwF32 (IEEE-754 binary32) comparison for equality.
 *
 * @note    This macro wrapper was introduced to allow quick implementation
 *          changes and aid profiling. Do NOT call native GCC code directly!!!
 *
 * @param[in]      _src1        The first comparison operand
 * @param[in]      _src2        The second comparison operand
 * 
 * @return LW_TRUE if _src1 == _src2
 * @return LW_FALSE otherwise
 * 
 * @note    On 324 MHz core comparisons take ~0.67us, ~0.40us on 540 MHz core.
 *          Figure is provided to get feeling how long this takes and future
 *          profiling efforts should measure time on actual code / data.
 */
#define lwF32CmpEQ(_src1, _src2)        ((_src1) == (_src2))

/*!
 * @brief   Macro performing LwF32 (IEEE-754 binary32) comparison for inequality.
 *
 * @note    This macro wrapper was introduced to allow quick implementation
 *          changes and aid profiling. Do NOT call native GCC code directly!!!
 *
 * @param[in]      _src1        The first comparison operand
 * @param[in]      _src2        The second comparison operand
 * 
 * @return LW_TRUE if _src1 != _src2
 * @return LW_FALSE otherwise
 * 
 * @note    On 324 MHz core comparisons take ~0.67us, ~0.40us on 540 MHz core.
 *          Figure is provided to get feeling how long this takes and future
 *          profiling efforts should measure time on actual code / data.
 */
#define lwF32CmpNE(_src1, _src2)        ((_src1) != (_src2))

/*!
 * @brief   Macro that checks if an LwF32 (IEEE-754 binary32) is less than another LwF32
 *
 * @note    This macro wrapper was introduced to allow quick implementation
 *          changes and aid profiling. Do NOT call native GCC code directly!!!
 *
 * @param[in]      _src1        The first comparison operand
 * @param[in]      _src2        The second comparison operand
 * 
 * @return LW_TRUE if _src1 < _src2
 * @return LW_FALSE otherwise
 * 
 * @note    On 324 MHz core comparisons take ~0.67us, ~0.40us on 540 MHz core.
 *          Figure is provided to get feeling how long this takes and future
 *          profiling efforts should measure time on actual code / data.
 */
#define lwF32CmpLT(_src1, _src2)        ((_src1) <  (_src2))

/*!
 * @brief   Macro that checks if an LwF32 (IEEE-754 binary32) is less than or equal to another LwF32
 *
 * @note    This macro wrapper was introduced to allow quick implementation
 *          changes and aid profiling. Do NOT call native GCC code directly!!!
 *
 * @param[in]      _src1        The first comparison operand
 * @param[in]      _src2        The second comparison operand
 * 
 * @return LW_TRUE if _src1 <= _src2
 * @return LW_FALSE otherwise
 * 
 * @note    On 324 MHz core comparisons take ~0.67us, ~0.40us on 540 MHz core.
 *          Figure is provided to get feeling how long this takes and future
 *          profiling efforts should measure time on actual code / data.
 */
#define lwF32CmpLE(_src1, _src2)        ((_src1) <= (_src2))

/*!
 * @brief   Macro that checks if an LwF32 (IEEE-754 binary32) is greater than another LwF32
 *
 * @note    This macro wrapper was introduced to allow quick implementation
 *          changes and aid profiling. Do NOT call native GCC code directly!!!
 *
 * @param[in]      _src1        The first comparison operand
 * @param[in]      _src2        The second comparison operand
 * 
 * @return LW_TRUE if _src1 > _src2
 * @return LW_FALSE otherwise
 * 
 * @note    On 324 MHz core comparisons take ~0.67us, ~0.40us on 540 MHz core.
 *          Figure is provided to get feeling how long this takes and future
 *          profiling efforts should measure time on actual code / data.
 */
#define lwF32CmpGT(_src1, _src2)        ((_src1) >  (_src2))

/*!
 * @brief   Macro that checks if an LwF32 (IEEE-754 binary32) is greater than or equal to another LwF32
 *
 * @note    This macro wrapper was introduced to allow quick implementation
 *          changes and aid profiling. Do NOT call native GCC code directly!!!
 *
 * @param[in]      _src1        The first comparison operand
 * @param[in]      _src2        The second comparison operand
 * 
 * @return LW_TRUE if _src1 >= _src2
 * @return LW_FALSE otherwise
 * 
 * @note    On 324 MHz core comparisons take ~0.67us, ~0.40us on 540 MHz core.
 *          Figure is provided to get feeling how long this takes and future
 *          profiling efforts should measure time on actual code / data.
 */
#define lwF32CmpGE(_src1, _src2)        ((_src1) >= (_src2))

/*!
 * @brief   Macro performing LwF32 (IEEE-754 binary32) addition.
 *
 * @param[in]      _src1        The first operand
 * @param[in]      _src2        The second operand
 * 
 * @return The sum of the operands.
 * 
 * @note    This macro wrapper was introduced to allow quick implementation
 *          changes and aid profiling. Do NOT call native GCC code directly!!!
 *
 * @note    On 324 MHz core ADD takes ~1.01us, ~0.57us on 540 MHz core.
 */
#define lwF32Add(_src1, _src2)          ((_src1) + (_src2))

/*!
 * @brief   Macro performing LwF32 (IEEE-754 binary32) subtraction.
 *
 * @param[in]      _src1        The first operand
 * @param[in]      _src2        The second operand
 * 
 * @return The difference of the operands.
 * 
 * @note    This macro wrapper was introduced to allow quick implementation
 *          changes and aid profiling. Do NOT call native GCC code directly!!!
 *
 * @note    On 324 MHz core SUB takes ~0.97us, ~0.59us on 540 MHz core.
 *          Figures are provided to get feeling how long this takes and future
 *          profiling efforts should measure time on actual code / data.
 */
#define lwF32Sub(_src1, _src2)          ((_src1) - (_src2))

/*!
 * @brief   Macro performing LwF32 (IEEE-754 binary32) multiplication.
 *
 * @param[in]      _src1        The first operand
 * @param[in]      _src2        The second operand
 * 
 * @return The product of the operands.
 * 
 * @note    This macro wrapper was introduced to allow quick implementation
 *          changes and aid profiling. Do NOT call native GCC code directly!!!
 *
 * @note    On 324 MHz core MUL takes ~1.54us, ~0.95us on 540 MHz core.
 *          Figures are provided to get feeling how long this takes and future
 *          profiling efforts should measure time on actual code / data.
 *
 * @pre     lwF32Mul() depends on 64-bit multiplication (.libLw64).
 */
#define lwF32Mul(_src1, _src2)          ((_src1) * (_src2))

/*!
 * @brief   Macro performing LwF32 (IEEE-754 binary32) division.
 *
 * @param[in]      _src1        The first operand
 * @param[in]      _src2        The second operand
 * 
 * @return The quotient of the operands.
 * 
 * @note    This macro wrapper was introduced to allow quick implementation
 *          changes and aid profiling. Do NOT call native GCC code directly!!!
 *
 * @note    On 324 MHz core DIV takes ~2.07us, ~1.77us on 540 MHz core.
 *          Figures are provided to get feeling how long this takes and future
 *          profiling efforts should measure time on actual code / data.
 *
 * @pre     lwF32Div() depend on 64-bit multiplication (.libLw64).
 */
#define lwF32Div(_src1, _src2)          ((_src1) / (_src2))

/* ------------------------- Global Variables ------------------------------- */

/* ------------------------- Public Functions ------------------------------- */
LwF32 lwF32ColwertFromU32(LwU32 value)
    GCC_ATTRIB_SECTION("imem_libLwF32", "lwF32ColwertFromU32");

LwF32 lwF32ColwertFromS32(LwS32 value)
    GCC_ATTRIB_SECTION("imem_libLwF32", "lwF32ColwertFromS32");

LwU32 lwF32ColwertToU32(LwF32 value)
    GCC_ATTRIB_SECTION("imem_libLwF32", "lwF32ColwertToU32");

LwS32 lwF32ColwertToS32(LwF32 value)
    GCC_ATTRIB_SECTION("imem_libLwF32", "lwF32ColwertToS32");

LwF32 lwF32MapFromU32(LwU32 *pValue)
    GCC_ATTRIB_SECTION("imem_libLwF32", "lwF32MapFromU32");

LwU32 lwF32MapToU32(LwF32 *pValue)
    GCC_ATTRIB_SECTION("imem_libLwF32", "lwF32MapToU32");

LwF32 lwF32ColwertFromU64(LwU64 value)
    GCC_ATTRIB_SECTION("imem_libLwF32", "lwF32ColwertFromU64");

LwU64 lwF32ColwertToU64(LwF32 value)
    GCC_ATTRIB_SECTION("imem_libLwF32", "lwF32ColwertToU64");

/*!
 * Helper Macro to colwert a LwF32 type to a UFXP_X_Y type
 * with rounding up by 0.5
 *
 * @param[in]      x        FXP fix part
 * @param[in]      y        FXP fractional part
 * @param[in]      floatVal   The input LwF32 value
 *
 * @return the UFXP_X_Y rounded value
 */
#define LW_TYPES_F32_TO_UFXP_X_Y_ROUND(x, y, floatVal)                                        \
        (LwUFXP##x##_##y)lwF32ColwertToU32(lwF32Add(lwF32Mul(floatVal,                        \
        (LwF32)(1U << DRF_SHIFT(LW_TYPES_FXP_INTEGER((x), (y))))),                            \
        (LwF32)0.5))

/*!
 * Helper to colwert a UFXP_X_Y type to a LwF32 type
 * with appropriate scaling
 *
 * @param[in]      x            FXP fix part
 * @param[in]      y            FXP fractional part
 * @param[in]      fxp          The input FXP_X_Y value
 *
 * @return the LwF32 value
 */
#define LW_TYPES_UFXP_X_Y_TO_F32(x, y, fxp)                                                   \
        (LwF32) lwF32Div(lwF32ColwertFromU32(fxp),                                            \
        (LwF32)(1U << DRF_SHIFT(LW_TYPES_FXP_INTEGER((x), (y)))))

/*!
 * Helper to colwert a SFXP_X_Y type to a LwF32 type
 * with appropriate scaling
 *
 * @param[in]      x            FXP fix part
 * @param[in]      y            FXP fractional part
 * @param[in]      fxp          The input FXP_X_Y value
 *
 * @return the LwF32 value
 */
#define LW_TYPES_SFXP_X_Y_TO_F32(x, y, fxp)                                                   \
        (LwF32) lwF32Div(lwF32ColwertFromS32(fxp),                                            \
        (LwF32)(1U << DRF_SHIFT(LW_TYPES_FXP_INTEGER((x), (y)))))

#endif // LIB_LWF32_H
