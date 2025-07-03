/*
 * Copyright 1993-2021 LWPU Corporation.  All rights reserved.
 *
 * NOTICE TO LICENSEE:
 *
 * This source code and/or documentation ("Licensed Deliverables") are
 * subject to LWPU intellectual property rights under U.S. and
 * international Copyright laws.
 *
 * These Licensed Deliverables contained herein is PROPRIETARY and
 * CONFIDENTIAL to LWPU and is being provided under the terms and
 * conditions of a form of LWPU software license agreement by and
 * between LWPU and Licensee ("License Agreement") or electronically
 * accepted by Licensee.  Notwithstanding any terms or conditions to
 * the contrary in the License Agreement, reproduction or disclosure
 * of the Licensed Deliverables to any third party without the express
 * written consent of LWPU is prohibited.
 *
 * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
 * LICENSE AGREEMENT, LWPU MAKES NO REPRESENTATION ABOUT THE
 * SUITABILITY OF THESE LICENSED DELIVERABLES FOR ANY PURPOSE.  IT IS
 * PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.
 * LWPU DISCLAIMS ALL WARRANTIES WITH REGARD TO THESE LICENSED
 * DELIVERABLES, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY,
 * NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
 * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
 * LICENSE AGREEMENT, IN NO EVENT SHALL LWPU BE LIABLE FOR ANY
 * SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THESE LICENSED DELIVERABLES.
 *
 * U.S. Government End Users.  These Licensed Deliverables are a
 * "commercial item" as that term is defined at 48 C.F.R. 2.101 (OCT
 * 1995), consisting of "commercial computer software" and "commercial
 * computer software documentation" as such terms are used in 48
 * C.F.R. 12.212 (SEPT 1995) and is provided to the U.S. Government
 * only as a commercial end item.  Consistent with 48 C.F.R.12.212 and
 * 48 C.F.R. 227.7202-1 through 227.7202-4 (JUNE 1995), all
 * U.S. Government End Users acquire the Licensed Deliverables with
 * only those rights set forth herein.
 *
 * Any use of the Licensed Deliverables in individual and commercial
 * software must include, in the user documentation and internal
 * comments to the code, the above Disclaimer and U.S. Government End
 * Users Notice.
 */

#if !defined(__LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS__)
#if defined(_MSC_VER)
#pragma message("crt/device_double_functions.h is an internal header file and must not be used directly.  Please use lwda_runtime_api.h or lwda_runtime.h instead.")
#else
#warning "crt/device_double_functions.h is an internal header file and must not be used directly.  Please use lwda_runtime_api.h or lwda_runtime.h instead."
#endif
#define __LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS__
#define __UNDEF_LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS_DEVICE_DOUBLE_FUNCTIONS_H__
#endif

#if !defined(__DEVICE_DOUBLE_FUNCTIONS_H__)
#define __DEVICE_DOUBLE_FUNCTIONS_H__

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if defined(__cplusplus) && defined(__LWDACC__)

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if defined(__LWDACC_RTC__)
#define __DEVICE_DOUBLE_FUNCTIONS_DECL__ __device__
#else
#define __DEVICE_DOUBLE_FUNCTIONS_DECL__ static __inline__ __device__
#endif /* __LWDACC_RTC__ */

#include "builtin_types.h"
#include "device_types.h"
#include "host_defines.h"

extern "C"
{
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Reinterpret bits in a double as a 64-bit signed integer.
 *
 * Reinterpret the bits in the double-precision floating-point value \p x
 * as a signed 64-bit integer.
 * \return Returns reinterpreted value.
 */
extern __device__ __device_builtin__ long long int         __double_as_longlong(double x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Reinterpret bits in a 64-bit signed integer as a double.
 *
 * Reinterpret the bits in the 64-bit signed integer value \p x as
 * a double-precision floating-point value.
 * \return Returns reinterpreted value.
 */
extern __device__ __device_builtin__ double                __longlong_as_double(long long int x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_DOUBLE
 * \brief Compute 
 * \latexonly $x \times y + z$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- &Multiply; --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>+</m:mo>
 *   <m:mi>z</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  as a single operation in round-to-nearest-even mode.
 *
 * Computes the value of 
 * \latexonly $x \times y + z$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- &Multiply; --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>+</m:mo>
 *   <m:mi>z</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  as a single ternary operation, rounding the
 * result once in round-to-nearest-even mode.
 *
 * \return Returns the rounded value of 
 * \latexonly $x \times y + z$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- &Multiply; --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>+</m:mo>
 *   <m:mi>z</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  as a single operation.
 * - fmaf(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- &PlusMinus; --></m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- &PlusMinus; --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p z) returns NaN.
 * - fmaf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- &PlusMinus; --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- &PlusMinus; --></m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p z) returns NaN.
 * - fmaf(\p x, \p y, 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>-</m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN if
 * \latexonly $x \times y$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- &Multiply; --></m:mo>
 *   <m:mi>y</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  is an exact 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * .
 * - fmaf(\p x, \p y, 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN if
 * \latexonly $x \times y$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- &Multiply; --></m:mo>
 *   <m:mi>y</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  is an exact 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>-</m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * .
 *
 * \note_aclwracy_double
 */
extern __device__ __device_builtin__ double                __fma_rn(double x, double y, double z);
/**
 * \ingroup LWDA_MATH_INTRINSIC_DOUBLE
 * \brief Compute 
 * \latexonly $x \times y + z$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- &Multiply; --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>+</m:mo>
 *   <m:mi>z</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  as a single operation in round-towards-zero mode.
 *
 * Computes the value of 
 * \latexonly $x \times y + z$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- &Multiply; --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>+</m:mo>
 *   <m:mi>z</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  as a single ternary operation, rounding the
 * result once in round-towards-zero mode.
 *
 * \return Returns the rounded value of 
 * \latexonly $x \times y + z$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- &Multiply; --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>+</m:mo>
 *   <m:mi>z</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  as a single operation.
 * - fmaf(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- &PlusMinus; --></m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- &PlusMinus; --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p z) returns NaN.
 * - fmaf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- &PlusMinus; --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- &PlusMinus; --></m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p z) returns NaN.
 * - fmaf(\p x, \p y, 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>-</m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN if
 * \latexonly $x \times y$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- &Multiply; --></m:mo>
 *   <m:mi>y</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  is an exact 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * .
 * - fmaf(\p x, \p y, 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN if
 * \latexonly $x \times y$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- &Multiply; --></m:mo>
 *   <m:mi>y</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  is an exact 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>-</m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * .
 *
 * \note_aclwracy_double
 */
extern __device__ __device_builtin__ double                __fma_rz(double x, double y, double z);
/**
 * \ingroup LWDA_MATH_INTRINSIC_DOUBLE
 * \brief Compute 
 * \latexonly $x \times y + z$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- &Multiply; --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>+</m:mo>
 *   <m:mi>z</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  as a single operation in round-up mode.
 *
 * Computes the value of 
 * \latexonly $x \times y + z$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- &Multiply; --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>+</m:mo>
 *   <m:mi>z</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  as a single ternary operation, rounding the
 * result once in round-up (to positive infinity) mode.
 *
 * \return Returns the rounded value of 
 * \latexonly $x \times y + z$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- &Multiply; --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>+</m:mo>
 *   <m:mi>z</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  as a single operation.
 * - fmaf(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- &PlusMinus; --></m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- &PlusMinus; --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p z) returns NaN.
 * - fmaf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- &PlusMinus; --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- &PlusMinus; --></m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p z) returns NaN.
 * - fmaf(\p x, \p y, 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>-</m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN if
 * \latexonly $x \times y$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- &Multiply; --></m:mo>
 *   <m:mi>y</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  is an exact 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * .
 * - fmaf(\p x, \p y, 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN if
 * \latexonly $x \times y$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- &Multiply; --></m:mo>
 *   <m:mi>y</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  is an exact 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>-</m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * .
 *
 * \note_aclwracy_double
 */
extern __device__ __device_builtin__ double                __fma_ru(double x, double y, double z);
/**
 * \ingroup LWDA_MATH_INTRINSIC_DOUBLE
 * \brief Compute 
 * \latexonly $x \times y + z$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- &Multiply; --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>+</m:mo>
 *   <m:mi>z</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  as a single operation in round-down mode.
 *
 * Computes the value of 
 * \latexonly $x \times y + z$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- &Multiply; --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>+</m:mo>
 *   <m:mi>z</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  as a single ternary operation, rounding the
 * result once in round-down (to negative infinity) mode.
 *
 * \return Returns the rounded value of 
 * \latexonly $x \times y + z$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- &Multiply; --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>+</m:mo>
 *   <m:mi>z</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  as a single operation.
 * - fmaf(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- &PlusMinus; --></m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- &PlusMinus; --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p z) returns NaN.
 * - fmaf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- &PlusMinus; --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- &PlusMinus; --></m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p z) returns NaN.
 * - fmaf(\p x, \p y, 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>-</m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN if
 * \latexonly $x \times y$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- &Multiply; --></m:mo>
 *   <m:mi>y</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  is an exact 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * .
 * - fmaf(\p x, \p y, 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN if
 * \latexonly $x \times y$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- &Multiply; --></m:mo>
 *   <m:mi>y</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  is an exact 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>-</m:mo>
 *   <m:mn>&#x221E;<!-- &Infinity; --></m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * .
 *
 * \note_aclwracy_double
 */
extern __device__ __device_builtin__ double                __fma_rd(double x, double y, double z);
/**
 * \ingroup LWDA_MATH_INTRINSIC_DOUBLE
 * \brief Add two floating-point values in round-to-nearest-even mode.
 *
 * Adds two floating-point values \p x and \p y in round-to-nearest-even mode.
 *
 * \return Returns \p x + \p y.
 *
 * \note_aclwracy_double
 * \note_nofma
 */
extern __device__ __device_builtin__ double                __dadd_rn(double x, double y);
/**      
 * \ingroup LWDA_MATH_INTRINSIC_DOUBLE
 * \brief Add two floating-point values in round-towards-zero mode.
 *
 * Adds two floating-point values \p x and \p y in round-towards-zero mode.
 *
 * \return Returns \p x + \p y.
 *
 * \note_aclwracy_double
 * \note_nofma
 */
extern __device__ __device_builtin__ double                __dadd_rz(double x, double y);
/**
 * \ingroup LWDA_MATH_INTRINSIC_DOUBLE
 * \brief Add two floating-point values in round-up mode.
 * 
 * Adds two floating-point values \p x and \p y in round-up (to positive infinity) mode.
 *    
 * \return Returns \p x + \p y.
 *
 * \note_aclwracy_double
 * \note_nofma
 */ 
extern __device__ __device_builtin__ double                __dadd_ru(double x, double y);
/**
 * \ingroup LWDA_MATH_INTRINSIC_DOUBLE
 * \brief Add two floating-point values in round-down mode.
 *
 * Adds two floating-point values \p x and \p y in round-down (to negative infinity) mode.
 *
 * \return Returns \p x + \p y.
 *
 * \note_aclwracy_double
 * \note_nofma
 */
extern __device__ __device_builtin__ double                __dadd_rd(double x, double y);
/**
 * \ingroup LWDA_MATH_INTRINSIC_DOUBLE
 * \brief Subtract two floating-point values in round-to-nearest-even mode.
 *
 * Subtracts two floating-point values \p x and \p y in round-to-nearest-even mode.
 *
 * \return Returns \p x - \p y.
 *
 * \note_aclwracy_double
 * \note_nofma
 */
extern __device__ __device_builtin__ double                __dsub_rn(double x, double y);
/**      
 * \ingroup LWDA_MATH_INTRINSIC_DOUBLE
 * \brief Subtract two floating-point values in round-towards-zero mode.
 *
 * Subtracts two floating-point values \p x and \p y in round-towards-zero mode.
 *
 * \return Returns \p x - \p y.
 *
 * \note_aclwracy_double
 * \note_nofma
 */
extern __device__ __device_builtin__ double                __dsub_rz(double x, double y);
/**
 * \ingroup LWDA_MATH_INTRINSIC_DOUBLE
 * \brief Subtract two floating-point values in round-up mode.
 * 
 * Subtracts two floating-point values \p x and \p y in round-up (to positive infinity) mode.
 *    
 * \return Returns \p x - \p y.
 *
 * \note_aclwracy_double
 * \note_nofma
 */ 
extern __device__ __device_builtin__ double                __dsub_ru(double x, double y);
/**
 * \ingroup LWDA_MATH_INTRINSIC_DOUBLE
 * \brief Subtract two floating-point values in round-down mode.
 *
 * Subtracts two floating-point values \p x and \p y in round-down (to negative infinity) mode.
 *
 * \return Returns \p x - \p y.
 *
 * \note_aclwracy_double
 * \note_nofma
 */
extern __device__ __device_builtin__ double                __dsub_rd(double x, double y);
/**
 * \ingroup LWDA_MATH_INTRINSIC_DOUBLE
 * \brief Multiply two floating-point values in round-to-nearest-even mode.
 *
 * Multiplies two floating-point values \p x and \p y in round-to-nearest-even mode.
 *
 * \return Returns \p x * \p y.
 *
 * \note_aclwracy_double
 * \note_nofma
 */
extern __device__ __device_builtin__ double                __dmul_rn(double x, double y);
/**      
 * \ingroup LWDA_MATH_INTRINSIC_DOUBLE
 * \brief Multiply two floating-point values in round-towards-zero mode.
 *
 * Multiplies two floating-point values \p x and \p y in round-towards-zero mode.
 *
 * \return Returns \p x * \p y.
 *
 * \note_aclwracy_double
 * \note_nofma
 */
extern __device__ __device_builtin__ double                __dmul_rz(double x, double y);
/**
 * \ingroup LWDA_MATH_INTRINSIC_DOUBLE
 * \brief Multiply two floating-point values in round-up mode.
 * 
 * Multiplies two floating-point values \p x and \p y in round-up (to positive infinity) mode.
 *    
 * \return Returns \p x * \p y.
 *
 * \note_aclwracy_double
 * \note_nofma
 */
extern __device__ __device_builtin__ double                __dmul_ru(double x, double y);
/**
 * \ingroup LWDA_MATH_INTRINSIC_DOUBLE
 * \brief Multiply two floating-point values in round-down mode.
 *
 * Multiplies two floating-point values \p x and \p y in round-down (to negative infinity) mode.
 *
 * \return Returns \p x * \p y.
 *
 * \note_aclwracy_double
 * \note_nofma
 */
extern __device__ __device_builtin__ double                __dmul_rd(double x, double y);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a double to a float in round-to-nearest-even mode.
 *
 * Colwert the double-precision floating-point value \p x to a single-precision
 * floating-point value in round-to-nearest-even mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ float                 __double2float_rn(double x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a double to a float in round-towards-zero mode.
 *
 * Colwert the double-precision floating-point value \p x to a single-precision
 * floating-point value in round-towards-zero mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ float                 __double2float_rz(double x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a double to a float in round-up mode.
 *
 * Colwert the double-precision floating-point value \p x to a single-precision
 * floating-point value in round-up (to positive infinity) mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ float                 __double2float_ru(double x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a double to a float in round-down mode.
 *
 * Colwert the double-precision floating-point value \p x to a single-precision
 * floating-point value in round-down (to negative infinity) mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ float                 __double2float_rd(double x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a double to a signed int in round-to-nearest-even mode.
 *
 * Colwert the double-precision floating-point value \p x to a
 * signed integer value in round-to-nearest-even mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ int                   __double2int_rn(double x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a double to a signed int in round-up mode.
 *
 * Colwert the double-precision floating-point value \p x to a
 * signed integer value in round-up (to positive infinity) mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ int                   __double2int_ru(double x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a double to a signed int in round-down mode.
 *
 * Colwert the double-precision floating-point value \p x to a
 * signed integer value in round-down (to negative infinity) mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ int                   __double2int_rd(double x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a double to an unsigned int in round-to-nearest-even mode.
 *
 * Colwert the double-precision floating-point value \p x to an
 * unsigned integer value in round-to-nearest-even mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ unsigned int          __double2uint_rn(double x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a double to an unsigned int in round-up mode.
 *
 * Colwert the double-precision floating-point value \p x to an
 * unsigned integer value in round-up (to positive infinity) mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ unsigned int          __double2uint_ru(double x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a double to an unsigned int in round-down mode.
 *
 * Colwert the double-precision floating-point value \p x to an
 * unsigned integer value in round-down (to negative infinity) mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ unsigned int          __double2uint_rd(double x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a double to a signed 64-bit int in round-to-nearest-even mode.
 *
 * Colwert the double-precision floating-point value \p x to a
 * signed 64-bit integer value in round-to-nearest-even mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ long long int          __double2ll_rn(double x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a double to a signed 64-bit int in round-up mode.
 *
 * Colwert the double-precision floating-point value \p x to a
 * signed 64-bit integer value in round-up (to positive infinity) mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ long long int          __double2ll_ru(double x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a double to a signed 64-bit int in round-down mode.
 *
 * Colwert the double-precision floating-point value \p x to a
 * signed 64-bit integer value in round-down (to negative infinity) mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ long long int          __double2ll_rd(double x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a double to an unsigned 64-bit int in round-to-nearest-even mode.
 *
 * Colwert the double-precision floating-point value \p x to an
 * unsigned 64-bit integer value in round-to-nearest-even mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ unsigned long long int __double2ull_rn(double x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a double to an unsigned 64-bit int in round-up mode.
 *
 * Colwert the double-precision floating-point value \p x to an
 * unsigned 64-bit integer value in round-up (to positive infinity) mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ unsigned long long int __double2ull_ru(double x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a double to an unsigned 64-bit int in round-down mode.
 *
 * Colwert the double-precision floating-point value \p x to an
 * unsigned 64-bit integer value in round-down (to negative infinity) mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ unsigned long long int __double2ull_rd(double x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a signed int to a double.
 *
 * Colwert the signed integer value \p x to a double-precision floating-point value.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ double                 __int2double_rn(int x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert an unsigned int to a double.
 *
 * Colwert the unsigned integer value \p x to a double-precision floating-point value.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ double                 __uint2double_rn(unsigned int x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a signed 64-bit int to a double in round-to-nearest-even mode.
 *
 * Colwert the signed 64-bit integer value \p x to a double-precision floating-point
 * value in round-to-nearest-even mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ double                 __ll2double_rn(long long int x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a signed 64-bit int to a double in round-towards-zero mode.
 *
 * Colwert the signed 64-bit integer value \p x to a double-precision floating-point
 * value in round-towards-zero mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ double                 __ll2double_rz(long long int x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a signed 64-bit int to a double in round-up mode.
 *
 * Colwert the signed 64-bit integer value \p x to a double-precision floating-point
 * value in round-up (to positive infinity) mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ double                 __ll2double_ru(long long int x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert a signed 64-bit int to a double in round-down mode.
 *
 * Colwert the signed 64-bit integer value \p x to a double-precision floating-point
 * value in round-down (to negative infinity) mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ double                 __ll2double_rd(long long int x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert an unsigned 64-bit int to a double in round-to-nearest-even mode.
 *
 * Colwert the unsigned 64-bit integer value \p x to a double-precision floating-point
 * value in round-to-nearest-even mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ double                 __ull2double_rn(unsigned long long int x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert an unsigned 64-bit int to a double in round-towards-zero mode.
 *
 * Colwert the unsigned 64-bit integer value \p x to a double-precision floating-point
 * value in round-towards-zero mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ double                 __ull2double_rz(unsigned long long int x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert an unsigned 64-bit int to a double in round-up mode.
 *
 * Colwert the unsigned 64-bit integer value \p x to a double-precision floating-point
 * value in round-up (to positive infinity) mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ double                 __ull2double_ru(unsigned long long int x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Colwert an unsigned 64-bit int to a double in round-down mode.
 *
 * Colwert the unsigned 64-bit integer value \p x to a double-precision floating-point
 * value in round-down (to negative infinity) mode.
 * \return Returns colwerted value.
 */
extern __device__ __device_builtin__ double                 __ull2double_rd(unsigned long long int x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Reinterpret high 32 bits in a double as a signed integer.
 *
 * Reinterpret the high 32 bits in the double-precision floating-point value \p x
 * as a signed integer.
 * \return Returns reinterpreted value.
 */
extern __device__ __device_builtin__ int                    __double2hiint(double x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Reinterpret low 32 bits in a double as a signed integer.
 *
 * Reinterpret the low 32 bits in the double-precision floating-point value \p x
 * as a signed integer.
 * \return Returns reinterpreted value.
 */
extern __device__ __device_builtin__ int                    __double2loint(double x);
/**
 * \ingroup LWDA_MATH_INTRINSIC_CAST
 * \brief Reinterpret high and low 32-bit integer values as a double.
 *
 * Reinterpret the integer value of \p hi as the high 32 bits of a 
 * double-precision floating-point value and the integer value of \p lo
 * as the low 32 bits of the same double-precision floating-point value.
 * \return Returns reinterpreted value.
 */
extern __device__ __device_builtin__ double                 __hiloint2double(int hi, int lo);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

__DEVICE_DOUBLE_FUNCTIONS_DECL__ double fma(double a, double b, double c, enum lwdaRoundMode mode);

__DEVICE_DOUBLE_FUNCTIONS_DECL__ double dmul(double a, double b, enum lwdaRoundMode mode = lwdaRoundNearest);

__DEVICE_DOUBLE_FUNCTIONS_DECL__ double dadd(double a, double b, enum lwdaRoundMode mode = lwdaRoundNearest);

__DEVICE_DOUBLE_FUNCTIONS_DECL__ double dsub(double a, double b, enum lwdaRoundMode mode = lwdaRoundNearest);

__DEVICE_DOUBLE_FUNCTIONS_DECL__ int double2int(double a, enum lwdaRoundMode mode = lwdaRoundZero);

__DEVICE_DOUBLE_FUNCTIONS_DECL__ unsigned int double2uint(double a, enum lwdaRoundMode mode = lwdaRoundZero);

__DEVICE_DOUBLE_FUNCTIONS_DECL__ long long int double2ll(double a, enum lwdaRoundMode mode = lwdaRoundZero);

__DEVICE_DOUBLE_FUNCTIONS_DECL__ unsigned long long int double2ull(double a, enum lwdaRoundMode mode = lwdaRoundZero);

__DEVICE_DOUBLE_FUNCTIONS_DECL__ double ll2double(long long int a, enum lwdaRoundMode mode = lwdaRoundNearest);

__DEVICE_DOUBLE_FUNCTIONS_DECL__ double ull2double(unsigned long long int a, enum lwdaRoundMode mode = lwdaRoundNearest);

__DEVICE_DOUBLE_FUNCTIONS_DECL__ double int2double(int a, enum lwdaRoundMode mode = lwdaRoundNearest);

__DEVICE_DOUBLE_FUNCTIONS_DECL__ double uint2double(unsigned int a, enum lwdaRoundMode mode = lwdaRoundNearest);

__DEVICE_DOUBLE_FUNCTIONS_DECL__ double float2double(float a, enum lwdaRoundMode mode = lwdaRoundNearest);

#undef __DEVICE_DOUBLE_FUNCTIONS_DECL__

#endif /* __cplusplus && __LWDACC__ */

#if !defined(__LWDACC_RTC__)
#include "device_double_functions.hpp"
#endif /* !__LWDACC_RTC__ */

#endif /* !__DEVICE_DOUBLE_FUNCTIONS_H__ */

#if defined(__UNDEF_LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS_DEVICE_DOUBLE_FUNCTIONS_H__)
#undef __LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS__
#undef __UNDEF_LWDA_INCLUDE_COMPILER_INTERNAL_HEADERS_DEVICE_DOUBLE_FUNCTIONS_H__
#endif
