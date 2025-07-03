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
 * @file  lib_fxp.h
 * @brief @copydoc lib_fxp.c
 */

#ifndef LIB_FXP_H
#define LIB_FXP_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
/* ------------------------- Function Prototypes --------------------------- */
/*!
 * @brief   Interface to facilitate multiplication of two unsigned 32-bit FXP
 *          numbers, keeping all of the operations within 32-bits, producing an
 *          output number which has been right-shifted by the specified number
 *          of bits.
 *
 * This operation discards (with rounding) the specified number of fractional
 * bits from the result and checks for an overflow in the integer bits, while
 * keeping all math within 32-bits.
 *
 * @param[in]   nShiftBits  Number of bits by which to right shift the result.
 *                          Expected to be in the range [0, 64).
 * @param[in]   x           First 32-bit operand
 * @param[in]   y           Second 32-bit operand
 *
 * @return  (x * y) >> nShiftBits   On success
 * @return  LW_U32_MAX              On overflow
 */
#define MultUFXP32(fname) LwUFXP32 (fname)(LwU8 nShiftBits, LwUFXP32 x, LwUFXP32 y)

/*!
 * @brief   Interface to facilitate multiplication of two unsigned 64-bit FXP
 *          numbers, keeping all of the operations within 64-bits, producing an
 *          output number which has been right-shifted by the specified number
 *          of bits.
 *
 * This operation discards (with rounding) the specified number of fractional
 * bits from the result and checks for an overflow in the integer bits, while
 * keeping all math within 64-bits.
 *
 * @param[in]   nShiftBits  Number of bits by which to right shift the result.
 *                          Expected to be in the range [0, 128).
 * @param[in]   x           First 64-bit operand
 * @param[in]   y           Second 64-bit operand
 *
 * @return  (x * y) >> nShiftBits   On success
 * @return  LW_U64_MAX              On overflow
 */
#define MultUFXP64(fname) LwU64 (fname)(LwU8 nShiftBits, LwU64 x, LwU64 y)

/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- Macros ---------------------------------------- */
/*!
 * Helper macro for multiplication of two LwUFXP20_12 variables, with result
 * returned as LwUFXP20_12 as well.  Wrapper to @ref multUFXP32().
 *
 * @param[in] x First coefficent
 * @param[in] y Second coefficent
 *
 * @return Product of x * y in original LwUFXP20_12 format. Fails if there is
 *         32-bit overflow.
 */
#define multUFXP20_12(x, y)                                                   \
    multUFXP32(12, x, y)

/*!
 * Helper macro for multiplication of two LwUFXP20_12 variables, with result
 * returned as LwUFXP20_12 as well.  Wrapper to @ref multUFXP32FailSafe().
 *
 * @param[in] x First coefficent
 * @param[in] y Second coefficent
 *
 * @return Product of x * y in original LwUFXP20_12 format if there is no
 *         32-bit overflow, LW_U32_MAX otherwise.
 */
#define multUFXP20_12FailSafe(x, y)                                           \
    multUFXP32FailSafe(12, x, y)

/*!
 * Helper macro for multiplication of two LwSFXP20_12 variables, with result
 * returned as LwSFXP20_12 as well.  Wrapper to @ref multSFXP32().
 *
 * @param[in] x First coefficent
 * @param[in] y Second coefficent
 *
 * @return Product of x * y in original LwSFXP20_12 format.
 */
#define multSFXP20_12(x, y)                                                   \
    multSFXP32(12, x, y)

/*!
 * Helper macro for multiplication of LwUFXP<a>_<b> with LwUFXP16_16 variable,
 * with result returned as LwUFXP<a>_<b>. Wrapper to @ref multUFXP32().
 *
 * @param[in]   x   First coefficent
 * @param[in]   y   Second coefficent
 *
 * @return  Product of x * y in original LwUFXP<a>_<b> format. Fails if there is
 *         32-bit overflow.
 */
#define multUFXP16_16(x, y)                                                   \
    multUFXP32(16, x, y)

/*!
 * Helper macro for multiplication of LwUFXP<a>_<b> with LwUFXP16_16 variable,
 * with result returned as LwUFXP<a>_<b>. Wrapper to @ref multUFXP32().
 *
 * @param[in]   x   First coefficent
 * @param[in]   y   Second coefficent
 *
 * @return  Product of x * y in original LwUFXP<a>_<b> format, if there is no
 *         32-bit overflow, LW_U32_MAX otherwise.
 */
#define multUFXP16_16FailSafe(x, y)                                           \
    multUFXP32FailSafe(16, x, y)

/*!
 * Helper macro for multiplication of LwSFXP<a>_<b> with LwSFXP16_16 variable,
 * with result returned as LwSFXP<a>_<b>. Wrapper to @ref multSFXP32().
 *
 * @param[in]   x   First coefficent
 * @param[in]   y   Second coefficent
 *
 * @return  Product of x * y in original LwSFXP<a>_<b> format.
 */
#define multSFXP16_16(x, y)                                                   \
    multSFXP32(16, x, y)

/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
MultUFXP32 (multUFXP32)
    GCC_ATTRIB_SECTION("imem_libFxpBasic", "multUFXP32");

MultUFXP32 (multUFXP32FailSafe)
    GCC_ATTRIB_SECTION("imem_libFxpBasic", "multUFXP32FailSafe");

MultUFXP64 (multUFXP64)
    GCC_ATTRIB_SECTION("imem_libFxpBasic", "multUFXP64");

/*!
 * Function to multiply to two signed 32-bit FXP numbers, keeping all of the
 * operations within 32-bits, producing an output number in the same FXP format.
 * This operation discards the format's number of fractional bits from the
 * result and checks for overflow in the integer bits.
 *
 * @param[in] nFracBits   Number of fractional bits in FXP format.
 * @param[in] x           First coefficent
 * @param[in] y           Second coefficent
 *
 * @return Product of x * y in original FXP format.
 */
LwSFXP32 multSFXP32(LwU8 nFracBits, LwSFXP32 x, LwSFXP32 y)
    GCC_ATTRIB_SECTION("imem_libFxpBasic", "multSFXP32");

/*!
 * Transcendental functions.
 */
LwUFXP20_12 powFXP(LwUFXP20_12 x, LwSFXP20_12 y)
    GCC_ATTRIB_SECTION("imem_libFxpExtended", "powFXP");
LwUFXP20_12 expFXP(LwSFXP20_12 x)
    GCC_ATTRIB_SECTION("imem_libFxpExtended", "expFXP");
LwSFXP20_12 lnFXP(LwUFXP20_12 x)
    GCC_ATTRIB_SECTION("imem_libFxpExtended", "lnFXP");

#endif // LIB_FXP_H
