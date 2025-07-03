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

/**
* \defgroup LWDA_MATH_INTRINSIC_BFLOAT16 Bfloat16 Precision Intrinsics
* This section describes lw_bfloat16 precision intrinsic functions that are
* only supported in device code.
* To use these functions, include the header file \p lwda_bf16.h in your program.
*/

/**
* \defgroup LWDA_MATH__BFLOAT16_ARITHMETIC Bfloat16 Arithmetic Functions
* \ingroup LWDA_MATH_INTRINSIC_BFLOAT16
* To use these functions, include the header file \p lwda_bf16.h in your program.
*/

/**
* \defgroup LWDA_MATH__BFLOAT162_ARITHMETIC Bfloat162 Arithmetic Functions
* \ingroup LWDA_MATH_INTRINSIC_BFLOAT16
* To use these functions, include the header file \p lwda_bf16.h in your program.
*/

/**
* \defgroup LWDA_MATH__BFLOAT16_COMPARISON Bfloat16 Comparison Functions
* \ingroup LWDA_MATH_INTRINSIC_BFLOAT16
* To use these functions, include the header file \p lwda_bf16.h in your program.
*/

/**
* \defgroup LWDA_MATH__BFLOAT162_COMPARISON Bfloat162 Comparison Functions
* \ingroup LWDA_MATH_INTRINSIC_BFLOAT16
* To use these functions, include the header file \p lwda_bf16.h in your program.
*/

/**
* \defgroup LWDA_MATH__BFLOAT16_MISC Bfloat16 Precision Colwersion and Data Movement
* \ingroup LWDA_MATH_INTRINSIC_BFLOAT16
* To use these functions, include the header file \p lwda_bf16.h in your program.
*/

/**
* \defgroup LWDA_MATH__BFLOAT16_FUNCTIONS Bfloat16 Math Functions
* \ingroup LWDA_MATH_INTRINSIC_BFLOAT16
* To use these functions, include the header file \p lwda_bf16.h in your program.
*/

/**
* \defgroup LWDA_MATH__BFLOAT162_FUNCTIONS Bfloat162 Math Functions
* \ingroup LWDA_MATH_INTRINSIC_BFLOAT16
* To use these functions, include the header file \p lwda_bf16.h in your program.
*/

#ifndef __LWDA_BF16_H__
#define __LWDA_BF16_H__

#define ___LWDA_BF16_STRINGIFY_INNERMOST(x) #x
#define __LWDA_BF16_STRINGIFY(x) ___LWDA_BF16_STRINGIFY_INNERMOST(x)

#if defined(__cplusplus)
#if defined(__LWDACC__)
#define __LWDA_BF16_DECL__ static __device__ __inline__
#define __LWDA_HOSTDEVICE_BF16_DECL__ static __host__ __device__ __inline__
#else
#define __LWDA_HOSTDEVICE_BF16_DECL__ static
#endif /* defined(__LWDACC__) */

#define __LWDA_BF16_TYPES_EXIST__

/* Forward-declaration of structures defined in "lwda_bf16.hpp" */

/**
 * \brief lw_bfloat16 datatype 
 * 
 * \details This structure implements the datatype for storing 
 * lw_bfloat16 floating-point numbers. The structure implements 
 * assignment operators and type colwersions. 16 bits are being 
 * used in total: 1 sign bit, 8 bits for the exponent, and 
 * the significand is being stored in 7 bits. The total 
 * precision is 8 bits.
 * 
 */
struct __lw_bfloat16;

/**
 * \brief lw_bfloat162 datatype
 * 
 * \details This structure implements the datatype for storing two 
 * lw_bfloat16 floating-point numbers. 
 * The structure implements assignment operators and type colwersions. 
 * 
 */
struct __lw_bfloat162;

/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwerts double number to lw_bfloat16 precision in round-to-nearest-even mode
* and returns \p lw_bfloat16 with colwerted value.
*
* \details Colwerts double number \p a to lw_bfloat16 precision in round-to-nearest-even mode.
* \param[in] a - double. Is only being read.
* \returns lw_bfloat16
* - \p a colwerted to lw_bfloat16.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ __lw_bfloat16 __double2bfloat16(const double a);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwerts float number to lw_bfloat16 precision in round-to-nearest-even mode
* and returns \p lw_bfloat16 with colwerted value. 
* 
* \details Colwerts float number \p a to lw_bfloat16 precision in round-to-nearest-even mode. 
* \param[in] a - float. Is only being read. 
* \returns lw_bfloat16
* - \p a colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ __lw_bfloat16 __float2bfloat16(const float a);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwerts float number to lw_bfloat16 precision in round-to-nearest-even mode
* and returns \p lw_bfloat16 with colwerted value.
*
* \details Colwerts float number \p a to lw_bfloat16 precision in round-to-nearest-even mode.
* \param[in] a - float. Is only being read. 
* \returns lw_bfloat16
* - \p a colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ __lw_bfloat16 __float2bfloat16_rn(const float a);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwerts float number to lw_bfloat16 precision in round-towards-zero mode
* and returns \p lw_bfloat16 with colwerted value.
* 
* \details Colwerts float number \p a to lw_bfloat16 precision in round-towards-zero mode.
* \param[in] a - float. Is only being read. 
* \returns lw_bfloat16
* - \p a colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ __lw_bfloat16 __float2bfloat16_rz(const float a);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwerts float number to lw_bfloat16 precision in round-down mode
* and returns \p lw_bfloat16 with colwerted value.
* 
* \details Colwerts float number \p a to lw_bfloat16 precision in round-down mode.
* \param[in] a - float. Is only being read. 
* 
* \returns lw_bfloat16
* - \p a colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ __lw_bfloat16 __float2bfloat16_rd(const float a);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwerts float number to lw_bfloat16 precision in round-up mode
* and returns \p lw_bfloat16 with colwerted value.
* 
* \details Colwerts float number \p a to lw_bfloat16 precision in round-up mode.
* \param[in] a - float. Is only being read. 
* 
* \returns lw_bfloat16
* - \p a colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ __lw_bfloat16 __float2bfloat16_ru(const float a);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwerts \p lw_bfloat16 number to float.
* 
* \details Colwerts lw_bfloat16 number \p a to float.
* \param[in] a - float. Is only being read. 
* 
* \returns float
* - \p a colwerted to float. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ float __bfloat162float(const __lw_bfloat16 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwerts input to lw_bfloat16 precision in round-to-nearest-even mode and
* populates both halves of \p lw_bfloat162 with colwerted value.
*
* \details Colwerts input \p a to lw_bfloat16 precision in round-to-nearest-even mode and
* populates both halves of \p lw_bfloat162 with colwerted value.
* \param[in] a - float. Is only being read. 
*
* \returns lw_bfloat162
* - The \p lw_bfloat162 value with both halves equal to the colwerted lw_bfloat16
* precision number.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ __lw_bfloat162 __float2bfloat162_rn(const float a);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwerts both input floats to lw_bfloat16 precision in round-to-nearest-even
* mode and returns \p lw_bfloat162 with colwerted values.
*
* \details Colwerts both input floats to lw_bfloat16 precision in round-to-nearest-even mode
* and combines the results into one \p lw_bfloat162 number. Low 16 bits of the return
* value correspond to the input \p a, high 16 bits correspond to the input \p
* b.
* \param[in] a - float. Is only being read. 
* \param[in] b - float. Is only being read. 
* 
* \returns lw_bfloat162
* - The \p lw_bfloat162 value with corresponding halves equal to the
* colwerted input floats.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ __lw_bfloat162 __floats2bfloat162_rn(const float a, const float b);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwerts low 16 bits of \p lw_bfloat162 to float and returns the result
* 
* \details Colwerts low 16 bits of \p lw_bfloat162 input \p a to 32-bit floating-point number
* and returns the result.
* \param[in] a - lw_bfloat162. Is only being read. 
* 
* \returns float
* - The low 16 bits of \p a colwerted to float.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ float __low2float(const __lw_bfloat162 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwerts high 16 bits of \p lw_bfloat162 to float and returns the result
* 
* \details Colwerts high 16 bits of \p lw_bfloat162 input \p a to 32-bit floating-point number
* and returns the result.
* \param[in] a - lw_bfloat162. Is only being read. 
* 
* \returns float
* - The high 16 bits of \p a colwerted to float.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ float __high2float(const __lw_bfloat162 a);

#if defined(__LWDACC__) && (__LWDA_ARCH__ >= 800 || !defined(__LWDA_ARCH__))
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwerts both components of float2 number to lw_bfloat16 precision in
* round-to-nearest-even mode and returns \p lw_bfloat162 with colwerted values.
* 
* \details Colwerts both components of float2 to lw_bfloat16 precision in round-to-nearest
* mode and combines the results into one \p lw_bfloat162 number. Low 16 bits of the
* return value correspond to \p a.x and high 16 bits of the return value
* correspond to \p a.y.
* \param[in] a - float2. Is only being read. 
*  
* \returns lw_bfloat162
* - The \p lw_bfloat162 which has corresponding halves equal to the
* colwerted float2 components.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ __lw_bfloat162 __float22bfloat162_rn(const float2 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwerts both halves of \p lw_bfloat162 to float2 and returns the result.
* 
* \details Colwerts both halves of \p lw_bfloat162 input \p a to float2 and returns the
* result.
* \param[in] a - lw_bfloat162. Is only being read. 
* 
* \returns float2
* - \p a colwerted to float2.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ float2 __bfloat1622float2(const __lw_bfloat162 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to a signed integer in round-to-nearest-even mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to a signed integer in
* round-to-nearest-even mode. NaN inputs are colwerted to 0.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns int
* - \p h colwerted to a signed integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ int __bfloat162int_rn(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to a signed integer in round-towards-zero mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to a signed integer in
* round-towards-zero mode. NaN inputs are colwerted to 0.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns int
* - \p h colwerted to a signed integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ int __bfloat162int_rz(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to a signed integer in round-down mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to a signed integer in
* round-down mode. NaN inputs are colwerted to 0.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns int
* - \p h colwerted to a signed integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ int __bfloat162int_rd(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to a signed integer in round-up mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to a signed integer in
* round-up mode. NaN inputs are colwerted to 0.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns int
* - \p h colwerted to a signed integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ int __bfloat162int_ru(const __lw_bfloat16 h);

/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a signed integer to a lw_bfloat16 in round-to-nearest-even mode.
* 
* \details Colwert the signed integer value \p i to a lw_bfloat16 floating-point
* value in round-to-nearest-even mode.
* \param[in] i - int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ __lw_bfloat16 __int2bfloat16_rn(const int i);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a signed integer to a lw_bfloat16 in round-towards-zero mode.
* 
* \details Colwert the signed integer value \p i to a lw_bfloat16 floating-point
* value in round-towards-zero mode.
* \param[in] i - int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __int2bfloat16_rz(const int i);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a signed integer to a lw_bfloat16 in round-down mode.
* 
* \details Colwert the signed integer value \p i to a lw_bfloat16 floating-point
* value in round-down mode.
* \param[in] i - int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __int2bfloat16_rd(const int i);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a signed integer to a lw_bfloat16 in round-up mode.
* 
* \details Colwert the signed integer value \p i to a lw_bfloat16 floating-point
* value in round-up mode.
* \param[in] i - int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __int2bfloat16_ru(const int i);

/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to a signed short integer in round-to-nearest-even
* mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to a signed short
* integer in round-to-nearest-even mode. NaN inputs are colwerted to 0.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns short int
* - \p h colwerted to a signed short integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ short int __bfloat162short_rn(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to a signed short integer in round-towards-zero mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to a signed short
* integer in round-towards-zero mode. NaN inputs are colwerted to 0.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns short int
* - \p h colwerted to a signed short integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ short int __bfloat162short_rz(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to a signed short integer in round-down mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to a signed short
* integer in round-down mode. NaN inputs are colwerted to 0.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns short int
* - \p h colwerted to a signed short integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ short int __bfloat162short_rd(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to a signed short integer in round-up mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to a signed short
* integer in round-up mode. NaN inputs are colwerted to 0.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns short int
* - \p h colwerted to a signed short integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ short int __bfloat162short_ru(const __lw_bfloat16 h);

/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a signed short integer to a lw_bfloat16 in round-to-nearest-even
* mode.
* 
* \details Colwert the signed short integer value \p i to a lw_bfloat16 floating-point
* value in round-to-nearest-even mode.
* \param[in] i - short int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ __lw_bfloat16 __short2bfloat16_rn(const short int i);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a signed short integer to a lw_bfloat16 in round-towards-zero mode.
* 
* \details Colwert the signed short integer value \p i to a lw_bfloat16 floating-point
* value in round-towards-zero mode.
* \param[in] i - short int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __short2bfloat16_rz(const short int i);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a signed short integer to a lw_bfloat16 in round-down mode.
* 
* \details Colwert the signed short integer value \p i to a lw_bfloat16 floating-point
* value in round-down mode.
* \param[in] i - short int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __short2bfloat16_rd(const short int i);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a signed short integer to a lw_bfloat16 in round-up mode.
* 
* \details Colwert the signed short integer value \p i to a lw_bfloat16 floating-point
* value in round-up mode.
* \param[in] i - short int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __short2bfloat16_ru(const short int i);

/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to an unsigned integer in round-to-nearest-even mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to an unsigned integer
* in round-to-nearest-even mode. NaN inputs are colwerted to 0.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns unsigned int
* - \p h colwerted to an unsigned integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ unsigned int __bfloat162uint_rn(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to an unsigned integer in round-towards-zero mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to an unsigned integer
* in round-towards-zero mode. NaN inputs are colwerted to 0.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns unsigned int
* - \p h colwerted to an unsigned integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ unsigned int __bfloat162uint_rz(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to an unsigned integer in round-down mode.
*
* \details Colwert the lw_bfloat16 floating-point value \p h to an unsigned integer
* in round-down mode. NaN inputs are colwerted to 0.
* \param[in] h - lw_bfloat16. Is only being read. 
*
* \returns unsigned int
* - \p h colwerted to an unsigned integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ unsigned int __bfloat162uint_rd(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to an unsigned integer in round-up mode.
*
* \details Colwert the lw_bfloat16 floating-point value \p h to an unsigned integer
* in round-up mode. NaN inputs are colwerted to 0.
* \param[in] h - lw_bfloat16. Is only being read. 
*
* \returns unsigned int
* - \p h colwerted to an unsigned integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ unsigned int __bfloat162uint_ru(const __lw_bfloat16 h);

/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert an unsigned integer to a lw_bfloat16 in round-to-nearest-even mode.
* 
* \details Colwert the unsigned integer value \p i to a lw_bfloat16 floating-point
* value in round-to-nearest-even mode.
* \param[in] i - unsigned int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ __lw_bfloat16 __uint2bfloat16_rn(const unsigned int i);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert an unsigned integer to a lw_bfloat16 in round-towards-zero mode.
* 
* \details Colwert the unsigned integer value \p i to a lw_bfloat16 floating-point
* value in round-towards-zero mode.
* \param[in] i - unsigned int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16.  
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __uint2bfloat16_rz(const unsigned int i);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert an unsigned integer to a lw_bfloat16 in round-down mode.
* 
* \details Colwert the unsigned integer value \p i to a lw_bfloat16 floating-point
* value in round-down mode.
* \param[in] i - unsigned int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __uint2bfloat16_rd(const unsigned int i);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert an unsigned integer to a lw_bfloat16 in round-up mode.
* 
* \details Colwert the unsigned integer value \p i to a lw_bfloat16 floating-point
* value in round-up mode.
* \param[in] i - unsigned int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __uint2bfloat16_ru(const unsigned int i);

/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to an unsigned short integer in round-to-nearest-even
* mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to an unsigned short
* integer in round-to-nearest-even mode. NaN inputs are colwerted to 0.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns unsigned short int
* - \p h colwerted to an unsigned short integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ unsigned short int __bfloat162ushort_rn(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to an unsigned short integer in round-towards-zero
* mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to an unsigned short
* integer in round-towards-zero mode. NaN inputs are colwerted to 0.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns unsigned short int
* - \p h colwerted to an unsigned short integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ unsigned short int __bfloat162ushort_rz(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to an unsigned short integer in round-down mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to an unsigned short
* integer in round-down mode. NaN inputs are colwerted to 0.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns unsigned short int
* - \p h colwerted to an unsigned short integer. 
*/
__LWDA_BF16_DECL__ unsigned short int __bfloat162ushort_rd(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to an unsigned short integer in round-up mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to an unsigned short
* integer in round-up mode. NaN inputs are colwerted to 0.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns unsigned short int
* - \p h colwerted to an unsigned short integer. 
*/
__LWDA_BF16_DECL__ unsigned short int __bfloat162ushort_ru(const __lw_bfloat16 h);

/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert an unsigned short integer to a lw_bfloat16 in round-to-nearest-even
* mode.
* 
* \details Colwert the unsigned short integer value \p i to a lw_bfloat16 floating-point
* value in round-to-nearest-even mode.
* \param[in] i - unsigned short int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ __lw_bfloat16 __ushort2bfloat16_rn(const unsigned short int i);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert an unsigned short integer to a lw_bfloat16 in round-towards-zero
* mode.
* 
* \details Colwert the unsigned short integer value \p i to a lw_bfloat16 floating-point
* value in round-towards-zero mode.
* \param[in] i - unsigned short int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __ushort2bfloat16_rz(const unsigned short int i);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert an unsigned short integer to a lw_bfloat16 in round-down mode.
* 
* \details Colwert the unsigned short integer value \p i to a lw_bfloat16 floating-point
* value in round-down mode.
* \param[in] i - unsigned short int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __ushort2bfloat16_rd(const unsigned short int i);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert an unsigned short integer to a lw_bfloat16 in round-up mode.
* 
* \details Colwert the unsigned short integer value \p i to a lw_bfloat16 floating-point
* value in round-up mode.
* \param[in] i - unsigned short int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __ushort2bfloat16_ru(const unsigned short int i);

/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to an unsigned 64-bit integer in round-to-nearest-even
* mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to an unsigned 64-bit
* integer in round-to-nearest-even mode. NaN inputs return 0x8000000000000000.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns unsigned long long int
* - \p h colwerted to an unsigned 64-bit integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ unsigned long long int __bfloat162ull_rn(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to an unsigned 64-bit integer in round-towards-zero
* mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to an unsigned 64-bit
* integer in round-towards-zero mode. NaN inputs return 0x8000000000000000.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns unsigned long long int
* - \p h colwerted to an unsigned 64-bit integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ unsigned long long int __bfloat162ull_rz(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to an unsigned 64-bit integer in round-down mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to an unsigned 64-bit
* integer in round-down mode. NaN inputs return 0x8000000000000000.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns unsigned long long int
* - \p h colwerted to an unsigned 64-bit integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ unsigned long long int __bfloat162ull_rd(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to an unsigned 64-bit integer in round-up mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to an unsigned 64-bit
* integer in round-up mode. NaN inputs return 0x8000000000000000.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns unsigned long long int
* - \p h colwerted to an unsigned 64-bit integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ unsigned long long int __bfloat162ull_ru(const __lw_bfloat16 h);

/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert an unsigned 64-bit integer to a lw_bfloat16 in round-to-nearest-even
* mode.
* 
* \details Colwert the unsigned 64-bit integer value \p i to a lw_bfloat16 floating-point
* value in round-to-nearest-even mode.
* \param[in] i - unsigned long long int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ __lw_bfloat16 __ull2bfloat16_rn(const unsigned long long int i);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert an unsigned 64-bit integer to a lw_bfloat16 in round-towards-zero
* mode.
* 
* \details Colwert the unsigned 64-bit integer value \p i to a lw_bfloat16 floating-point
* value in round-towards-zero mode.
* \param[in] i - unsigned long long int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __ull2bfloat16_rz(const unsigned long long int i);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert an unsigned 64-bit integer to a lw_bfloat16 in round-down mode.
* 
* \details Colwert the unsigned 64-bit integer value \p i to a lw_bfloat16 floating-point
* value in round-down mode.
* \param[in] i - unsigned long long int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16.  
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __ull2bfloat16_rd(const unsigned long long int i);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert an unsigned 64-bit integer to a lw_bfloat16 in round-up mode.
* 
* \details Colwert the unsigned 64-bit integer value \p i to a lw_bfloat16 floating-point
* value in round-up mode.
* \param[in] i - unsigned long long int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __ull2bfloat16_ru(const unsigned long long int i);

/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to a signed 64-bit integer in round-to-nearest-even
* mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to a signed 64-bit
* integer in round-to-nearest-even mode. NaN inputs return a long long int with hex value of 0x8000000000000000.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns long long int
* - \p h colwerted to a signed 64-bit integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ long long int __bfloat162ll_rn(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to a signed 64-bit integer in round-towards-zero mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to a signed 64-bit
* integer in round-towards-zero mode. NaN inputs return a long long int with hex value of 0x8000000000000000.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns long long int
* - \p h colwerted to a signed 64-bit integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ long long int __bfloat162ll_rz(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to a signed 64-bit integer in round-down mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to a signed 64-bit
* integer in round-down mode. NaN inputs return a long long int with hex value of 0x8000000000000000.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns long long int
* - \p h colwerted to a signed 64-bit integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ long long int __bfloat162ll_rd(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a lw_bfloat16 to a signed 64-bit integer in round-up mode.
* 
* \details Colwert the lw_bfloat16 floating-point value \p h to a signed 64-bit
* integer in round-up mode. NaN inputs return a long long int with hex value of 0x8000000000000000.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns long long int
* - \p h colwerted to a signed 64-bit integer. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ long long int __bfloat162ll_ru(const __lw_bfloat16 h);

/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a signed 64-bit integer to a lw_bfloat16 in round-to-nearest-even
* mode.
* 
* \details Colwert the signed 64-bit integer value \p i to a lw_bfloat16 floating-point
* value in round-to-nearest-even mode.
* \param[in] i - long long int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_HOSTDEVICE_BF16_DECL__ __lw_bfloat16 __ll2bfloat16_rn(const long long int i);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a signed 64-bit integer to a lw_bfloat16 in round-towards-zero mode.
* 
* \details Colwert the signed 64-bit integer value \p i to a lw_bfloat16 floating-point
* value in round-towards-zero mode.
* \param[in] i - long long int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __ll2bfloat16_rz(const long long int i);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a signed 64-bit integer to a lw_bfloat16 in round-down mode.
* 
* \details Colwert the signed 64-bit integer value \p i to a lw_bfloat16 floating-point
* value in round-down mode.
* \param[in] i - long long int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __ll2bfloat16_rd(const long long int i);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Colwert a signed 64-bit integer to a lw_bfloat16 in round-up mode.
* 
* \details Colwert the signed 64-bit integer value \p i to a lw_bfloat16 floating-point
* value in round-up mode.
* \param[in] i - long long int. Is only being read. 
* 
* \returns lw_bfloat16
* - \p i colwerted to lw_bfloat16. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __ll2bfloat16_ru(const long long int i);

/**
* \ingroup LWDA_MATH__BFLOAT16_FUNCTIONS
* \brief Truncate input argument to the integral part.
* 
* \details Round \p h to the nearest integer value that does not exceed \p h in
* magnitude.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns lw_bfloat16
* - The truncated integer value. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 htrunc(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_FUNCTIONS
* \brief Callwlate ceiling of the input argument.
* 
* \details Compute the smallest integer value not less than \p h.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns lw_bfloat16
* - The smallest integer value not less than \p h. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 hceil(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_FUNCTIONS
* \brief Callwlate the largest integer less than or equal to \p h.
* 
* \details Callwlate the largest integer value which is less than or equal to \p h.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns lw_bfloat16
* - The largest integer value which is less than or equal to \p h. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 hfloor(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_FUNCTIONS
* \brief Round input to nearest integer value in lw_bfloat16 floating-point
* number.
* 
* \details Round \p h to the nearest integer value in lw_bfloat16 floating-point
* format, with bfloat16way cases rounded to the nearest even integer value.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns lw_bfloat16
* - The nearest integer to \p h. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 hrint(const __lw_bfloat16 h);

/**
* \ingroup LWDA_MATH__BFLOAT162_FUNCTIONS
* \brief Truncate \p lw_bfloat162 vector input argument to the integral part.
* 
* \details Round each component of vector \p h to the nearest integer value that does
* not exceed \p h in magnitude.
* \param[in] h - lw_bfloat162. Is only being read. 
* 
* \returns lw_bfloat162
* - The truncated \p h. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 h2trunc(const __lw_bfloat162 h);
/**
* \ingroup LWDA_MATH__BFLOAT162_FUNCTIONS
* \brief Callwlate \p lw_bfloat162 vector ceiling of the input argument.
* 
* \details For each component of vector \p h compute the smallest integer value not less
* than \p h.
* \param[in] h - lw_bfloat162. Is only being read. 
* 
* \returns lw_bfloat162
* - The vector of smallest integers not less than \p h. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 h2ceil(const __lw_bfloat162 h);
/**
* \ingroup LWDA_MATH__BFLOAT162_FUNCTIONS
* \brief Callwlate the largest integer less than or equal to \p h.
* 
* \details For each component of vector \p h callwlate the largest integer value which
* is less than or equal to \p h.
* \param[in] h - lw_bfloat162. Is only being read. 
* 
* \returns lw_bfloat162
* - The vector of largest integers which is less than or equal to \p h. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 h2floor(const __lw_bfloat162 h);
/**
* \ingroup LWDA_MATH__BFLOAT162_FUNCTIONS
* \brief Round input to nearest integer value in lw_bfloat16 floating-point
* number.
* 
* \details Round each component of \p lw_bfloat162 vector \p h to the nearest integer value in
* lw_bfloat16 floating-point format, with bfloat16way cases rounded to the
* nearest even integer value.
* \param[in] h - lw_bfloat162. Is only being read. 
* 
* \returns lw_bfloat162
* - The vector of rounded integer values. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 h2rint(const __lw_bfloat162 h);

/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Returns \p lw_bfloat162 with both halves equal to the input value.
* 
* \details Returns \p lw_bfloat162 number with both halves equal to the input \p a \p lw_bfloat16
* number.
* \param[in] a - lw_bfloat16. Is only being read. 
* 
* \returns lw_bfloat162
* - The vector which has both its halves equal to the input \p a. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __bfloat162bfloat162(const __lw_bfloat16 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Swaps both halves of the \p lw_bfloat162 input.
* 
* \details Swaps both halves of the \p lw_bfloat162 input and returns a new \p lw_bfloat162 number
* with swapped halves.
* \param[in] a - lw_bfloat162. Is only being read. 
* 
* \returns lw_bfloat162
* - \p a with its halves being swapped. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __lowhigh2highlow(const __lw_bfloat162 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Extracts low 16 bits from each of the two \p lw_bfloat162 inputs and combines
* into one \p lw_bfloat162 number. 
* 
* \details Extracts low 16 bits from each of the two \p lw_bfloat162 inputs and combines into
* one \p lw_bfloat162 number. Low 16 bits from input \p a is stored in low 16 bits of
* the return value, low 16 bits from input \p b is stored in high 16 bits of
* the return value. 
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
* 
* \returns lw_bfloat162
* - The low 16 bits of \p a and of \p b. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __lows2bfloat162(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Extracts high 16 bits from each of the two \p lw_bfloat162 inputs and
* combines into one \p lw_bfloat162 number.
* 
* \details Extracts high 16 bits from each of the two \p lw_bfloat162 inputs and combines into
* one \p lw_bfloat162 number. High 16 bits from input \p a is stored in low 16 bits of
* the return value, high 16 bits from input \p b is stored in high 16 bits of
* the return value.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
* 
* \returns lw_bfloat162
* - The high 16 bits of \p a and of \p b. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __highs2bfloat162(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Returns high 16 bits of \p lw_bfloat162 input.
*
* \details Returns high 16 bits of \p lw_bfloat162 input \p a.
* \param[in] a - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat16
* - The high 16 bits of the input. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __high2bfloat16(const __lw_bfloat162 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Returns low 16 bits of \p lw_bfloat162 input.
*
* \details Returns low 16 bits of \p lw_bfloat162 input \p a.
* \param[in] a - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat16
* - Returns \p lw_bfloat16 which contains low 16 bits of the input \p a. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __low2bfloat16(const __lw_bfloat162 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_COMPARISON
* \brief Checks if the input \p lw_bfloat16 number is infinite.
* 
* \details Checks if the input \p lw_bfloat16 number \p a is infinite. 
* \param[in] a - lw_bfloat16. Is only being read. 
* 
* \returns int 
* - -1 iff \p a is equal to negative infinity, 
* - 1 iff \p a is equal to positive infinity, 
* - 0 otherwise. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ int __hisinf(const __lw_bfloat16 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Combines two \p lw_bfloat16 numbers into one \p lw_bfloat162 number.
* 
* \details Combines two input \p lw_bfloat16 number \p a and \p b into one \p lw_bfloat162 number.
* Input \p a is stored in low 16 bits of the return value, input \p b is stored
* in high 16 bits of the return value.
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
* 
* \returns lw_bfloat162
* - The lw_bfloat162 with one lw_bfloat16 equal to \p a and the other to \p b. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __halves2bfloat162(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Extracts low 16 bits from \p lw_bfloat162 input.
* 
* \details Extracts low 16 bits from \p lw_bfloat162 input \p a and returns a new \p lw_bfloat162
* number which has both halves equal to the extracted bits.
* \param[in] a - lw_bfloat162. Is only being read. 
* 
* \returns lw_bfloat162
* - The lw_bfloat162 with both halves equal to the low 16 bits of the input. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __low2bfloat162(const __lw_bfloat162 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Extracts high 16 bits from \p lw_bfloat162 input.
* 
* \details Extracts high 16 bits from \p lw_bfloat162 input \p a and returns a new \p lw_bfloat162
* number which has both halves equal to the extracted bits.
* \param[in] a - lw_bfloat162. Is only being read. 
* 
* \returns lw_bfloat162
* - The lw_bfloat162 with both halves equal to the high 16 bits of the input. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __high2bfloat162(const __lw_bfloat162 a);

/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Reinterprets bits in a \p lw_bfloat16 as a signed short integer.
* 
* \details Reinterprets the bits in the lw_bfloat16 floating-point number \p h
* as a signed short integer. 
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns short int
* - The reinterpreted value. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ short int __bfloat16_as_short(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Reinterprets bits in a \p lw_bfloat16 as an unsigned short integer.
* 
* \details Reinterprets the bits in the lw_bfloat16 floating-point \p h
* as an unsigned short number.
* \param[in] h - lw_bfloat16. Is only being read. 
* 
* \returns unsigned short int
* - The reinterpreted value.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ unsigned short int __bfloat16_as_ushort(const __lw_bfloat16 h);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Reinterprets bits in a signed short integer as a \p lw_bfloat16.
* 
* \details Reinterprets the bits in the signed short integer \p i as a
* lw_bfloat16 floating-point number.
* \param[in] i - short int. Is only being read. 
* 
* \returns lw_bfloat16
* - The reinterpreted value.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __short_as_bfloat16(const short int i);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Reinterprets bits in an unsigned short integer as a \p lw_bfloat16.
* 
* \details Reinterprets the bits in the unsigned short integer \p i as a
* lw_bfloat16 floating-point number.
* \param[in] i - unsigned short int. Is only being read. 
* 
* \returns lw_bfloat16
* - The reinterpreted value.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __ushort_as_bfloat16(const unsigned short int i);

#if !defined warpSize && !defined __local_warpSize
#define warpSize    32
#define __local_warpSize
#endif

/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Exchange a variable between threads within a warp. Direct copy from indexed thread. 
* 
* \details Returns the value of var held by the thread whose ID is given by delta. 
* If width is less than warpSize then each subsection of the warp behaves as a separate 
* entity with a starting logical thread ID of 0. If delta is outside the range [0:width-1], 
* the value returned corresponds to the value of var held by the delta modulo width (i.e. 
* within the same subsection). width must have a value which is a power of 2; 
* results are undefined if width is not a power of 2, or is a number greater than 
* warpSize. 
* \param[in] mask - unsigned int. Is only being read. 
* \param[in] var - lw_bfloat162. Is only being read. 
* \param[in] delta - int. Is only being read. 
* \param[in] width - int. Is only being read. 
* 
* \returns Returns the 4-byte word referenced by var from the source thread ID as lw_bfloat162. 
* If the source thread ID is out of range or the source thread has exited, the calling thread's own var is returned. 
* \note_ref_guide_warp_shuffle
* \internal
* \exception-guarantee no-throw guarantee
* \behavior not reentrant, not thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __shfl_sync(const unsigned mask, const __lw_bfloat162 var, const int delta, const int width = warpSize);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Exchange a variable between threads within a warp. Copy from a thread with lower ID relative to the caller. 
* 
* \details Callwlates a source thread ID by subtracting delta from the caller's lane ID. 
* The value of var held by the resulting lane ID is returned: in effect, var is shifted up 
* the warp by delta threads. If width is less than warpSize then each subsection of the warp 
* behaves as a separate entity with a starting logical thread ID of 0. The source thread index 
* will not wrap around the value of width, so effectively the lower delta threads will be unchanged. 
* width must have a value which is a power of 2; results are undefined if width is not a power of 2, 
* or is a number greater than warpSize. 
* \param[in] mask - unsigned int. Is only being read. 
* \param[in] var - lw_bfloat162. Is only being read. 
* \param[in] delta - int. Is only being read. 
* \param[in] width - int. Is only being read. 
* 
* \returns Returns the 4-byte word referenced by var from the source thread ID as lw_bfloat162. 
* If the source thread ID is out of range or the source thread has exited, the calling thread's own var is returned. 
* \note_ref_guide_warp_shuffle
* \internal
* \exception-guarantee no-throw guarantee
* \behavior not reentrant, not thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __shfl_up_sync(const unsigned mask, const __lw_bfloat162 var, const unsigned int delta, const int width = warpSize);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Exchange a variable between threads within a warp. Copy from a thread with higher ID relative to the caller. 
* 
* \details Callwlates a source thread ID by adding delta to the caller's thread ID. 
* The value of var held by the resulting thread ID is returned: this has the effect 
* of shifting var down the warp by delta threads. If width is less than warpSize then 
* each subsection of the warp behaves as a separate entity with a starting logical 
* thread ID of 0. As for __shfl_up_sync(), the ID number of the source thread 
* will not wrap around the value of width and so the upper delta threads 
* will remain unchanged. 
* \param[in] mask - unsigned int. Is only being read. 
* \param[in] var - lw_bfloat162. Is only being read. 
* \param[in] delta - int. Is only being read. 
* \param[in] width - int. Is only being read. 
* 
* \returns Returns the 4-byte word referenced by var from the source thread ID as lw_bfloat162. 
* If the source thread ID is out of range or the source thread has exited, the calling thread's own var is returned. 
* \note_ref_guide_warp_shuffle
* \internal
* \exception-guarantee no-throw guarantee
* \behavior not reentrant, not thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __shfl_down_sync(const unsigned mask, const __lw_bfloat162 var, const unsigned int delta, const int width = warpSize);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Exchange a variable between threads within a warp. Copy from a thread based on bitwise XOR of own thread ID. 
* 
* \details Callwlates a source thread ID by performing a bitwise XOR of the caller's thread ID with mask: 
* the value of var held by the resulting thread ID is returned. If width is less than warpSize then each 
* group of width conselwtive threads are able to access elements from earlier groups of threads, 
* however if they attempt to access elements from later groups of threads their own value of var 
* will be returned. This mode implements a butterfly addressing pattern such as is used in tree 
* reduction and broadcast. 
* \param[in] mask - unsigned int. Is only being read. 
* \param[in] var - lw_bfloat162. Is only being read. 
* \param[in] delta - int. Is only being read. 
* \param[in] width - int. Is only being read. 
* 
* \returns Returns the 4-byte word referenced by var from the source thread ID as lw_bfloat162. 
* If the source thread ID is out of range or the source thread has exited, the calling thread's own var is returned. 
* \note_ref_guide_warp_shuffle
* \internal
* \exception-guarantee no-throw guarantee
* \behavior not reentrant, not thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __shfl_xor_sync(const unsigned mask, const __lw_bfloat162 var, const int delta, const int width = warpSize);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Exchange a variable between threads within a warp. Direct copy from indexed thread. 
* 
* \details Returns the value of var held by the thread whose ID is given by delta. 
* If width is less than warpSize then each subsection of the warp behaves as a separate 
* entity with a starting logical thread ID of 0. If delta is outside the range [0:width-1], 
* the value returned corresponds to the value of var held by the delta modulo width (i.e. 
* within the same subsection). width must have a value which is a power of 2; 
* results are undefined if width is not a power of 2, or is a number greater than 
* warpSize. 
* \param[in] mask - unsigned int. Is only being read. 
* \param[in] var - lw_bfloat16. Is only being read. 
* \param[in] delta - int. Is only being read. 
* \param[in] width - int. Is only being read. 
* 
* \returns Returns the 2-byte word referenced by var from the source thread ID as lw_bfloat16. 
* If the source thread ID is out of range or the source thread has exited, the calling thread's own var is returned. 
* \note_ref_guide_warp_shuffle
* \internal
* \exception-guarantee no-throw guarantee
* \behavior not reentrant, not thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __shfl_sync(const unsigned mask, const __lw_bfloat16 var, const int delta, const int width = warpSize);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Exchange a variable between threads within a warp. Copy from a thread with lower ID relative to the caller. 
* \details Callwlates a source thread ID by subtracting delta from the caller's lane ID. 
* The value of var held by the resulting lane ID is returned: in effect, var is shifted up 
* the warp by delta threads. If width is less than warpSize then each subsection of the warp 
* behaves as a separate entity with a starting logical thread ID of 0. The source thread index 
* will not wrap around the value of width, so effectively the lower delta threads will be unchanged. 
* width must have a value which is a power of 2; results are undefined if width is not a power of 2, 
* or is a number greater than warpSize. 
* \param[in] mask - unsigned int. Is only being read. 
* \param[in] var - lw_bfloat16. Is only being read. 
* \param[in] delta - int. Is only being read. 
* \param[in] width - int. Is only being read. 
* 
* \returns Returns the 2-byte word referenced by var from the source thread ID as lw_bfloat16. 
* If the source thread ID is out of range or the source thread has exited, the calling thread's own var is returned. 
* \note_ref_guide_warp_shuffle
* \internal
* \exception-guarantee no-throw guarantee
* \behavior not reentrant, not thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __shfl_up_sync(const unsigned mask, const __lw_bfloat16 var, const unsigned int delta, const int width = warpSize);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Exchange a variable between threads within a warp. Copy from a thread with higher ID relative to the caller. 
* 
* \details Callwlates a source thread ID by adding delta to the caller's thread ID. 
* The value of var held by the resulting thread ID is returned: this has the effect 
* of shifting var down the warp by delta threads. If width is less than warpSize then 
* each subsection of the warp behaves as a separate entity with a starting logical 
* thread ID of 0. As for __shfl_up_sync(), the ID number of the source thread 
* will not wrap around the value of width and so the upper delta threads 
* will remain unchanged. 
* \param[in] mask - unsigned int. Is only being read. 
* \param[in] var - lw_bfloat16. Is only being read. 
* \param[in] delta - int. Is only being read. 
* \param[in] width - int. Is only being read. 
* 
* \returns Returns the 2-byte word referenced by var from the source thread ID as lw_bfloat16. 
* If the source thread ID is out of range or the source thread has exited, the calling thread's own var is returned. 
* \note_ref_guide_warp_shuffle
* \internal
* \exception-guarantee no-throw guarantee
* \behavior not reentrant, not thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __shfl_down_sync(const unsigned mask, const __lw_bfloat16 var, const unsigned int delta, const int width = warpSize);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Exchange a variable between threads within a warp. Copy from a thread based on bitwise XOR of own thread ID. 
* 
* \details Callwlates a source thread ID by performing a bitwise XOR of the caller's thread ID with mask: 
* the value of var held by the resulting thread ID is returned. If width is less than warpSize then each 
* group of width conselwtive threads are able to access elements from earlier groups of threads, 
* however if they attempt to access elements from later groups of threads their own value of var 
* will be returned. This mode implements a butterfly addressing pattern such as is used in tree 
* reduction and broadcast. 
* \param[in] mask - unsigned int. Is only being read. 
* \param[in] var - lw_bfloat16. Is only being read. 
* \param[in] delta - int. Is only being read. 
* \param[in] width - int. Is only being read. 
* 
* \returns Returns the 2-byte word referenced by var from the source thread ID as lw_bfloat16. 
* If the source thread ID is out of range or the source thread has exited, the calling thread's own var is returned. 
* \note_ref_guide_warp_shuffle
* \internal
* \exception-guarantee no-throw guarantee
* \behavior not reentrant, not thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __shfl_xor_sync(const unsigned mask, const __lw_bfloat16 var, const int delta, const int width = warpSize);

#if defined(__local_warpSize)
#undef warpSize
#undef __local_warpSize
#endif

/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Generates a `ld.global.nc` load instruction.
* \param[in] ptr - memory location
* \returns The value pointed by `ptr`
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __ldg(const  __lw_bfloat162 *const ptr);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Generates a `ld.global.nc` load instruction.
* \param[in] ptr - memory location
* \returns The value pointed by `ptr`
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __ldg(const __lw_bfloat16 *const ptr);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Generates a `ld.global.cg` load instruction.
* \param[in] ptr - memory location
* \returns The value pointed by `ptr`
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __ldcg(const  __lw_bfloat162 *const ptr);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Generates a `ld.global.cg` load instruction.
* \param[in] ptr - memory location
* \returns The value pointed by `ptr`
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __ldcg(const __lw_bfloat16 *const ptr);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Generates a `ld.global.ca` load instruction.
* \param[in] ptr - memory location
* \returns The value pointed by `ptr`
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __ldca(const  __lw_bfloat162 *const ptr);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Generates a `ld.global.ca` load instruction.
* \param[in] ptr - memory location
* \returns The value pointed by `ptr`
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __ldca(const __lw_bfloat16 *const ptr);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Generates a `ld.global.cs` load instruction.
* \param[in] ptr - memory location
* \returns The value pointed by `ptr`
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __ldcs(const  __lw_bfloat162 *const ptr);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Generates a `ld.global.cs` load instruction.
* \param[in] ptr - memory location
* \returns The value pointed by `ptr`
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __ldcs(const __lw_bfloat16 *const ptr);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Generates a `ld.global.lu` load instruction.
* \param[in] ptr - memory location
* \returns The value pointed by `ptr`
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __ldlu(const  __lw_bfloat162 *const ptr);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Generates a `ld.global.lu` load instruction.
* \param[in] ptr - memory location
* \returns The value pointed by `ptr`
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __ldlu(const __lw_bfloat16 *const ptr);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Generates a `ld.global.cv` load instruction.
* \param[in] ptr - memory location
* \returns The value pointed by `ptr`
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __ldcv(const  __lw_bfloat162 *const ptr);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Generates a `ld.global.cv` load instruction.
* \param[in] ptr - memory location
* \returns The value pointed by `ptr`
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __ldcv(const __lw_bfloat16 *const ptr);

/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Generates a `st.global.wb` store instruction.
* \param[out] ptr - memory location
* \param[in] value - the value to be stored
*/
__LWDA_BF16_DECL__ void __stwb(__lw_bfloat162 *const ptr, const __lw_bfloat162 value);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Generates a `st.global.wb` store instruction.
* \param[out] ptr - memory location
* \param[in] value - the value to be stored
*/
__LWDA_BF16_DECL__ void __stwb(__lw_bfloat16 *const ptr, const __lw_bfloat16 value);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Generates a `st.global.cg` store instruction.
* \param[out] ptr - memory location
* \param[in] value - the value to be stored
*/
__LWDA_BF16_DECL__ void __stcg(__lw_bfloat162 *const ptr, const __lw_bfloat162 value);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Generates a `st.global.cg` store instruction.
* \param[out] ptr - memory location
* \param[in] value - the value to be stored
*/
__LWDA_BF16_DECL__ void __stcg(__lw_bfloat16 *const ptr, const __lw_bfloat16 value);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Generates a `st.global.cs` store instruction.
* \param[out] ptr - memory location
* \param[in] value - the value to be stored
*/
__LWDA_BF16_DECL__ void __stcs(__lw_bfloat162 *const ptr, const __lw_bfloat162 value);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Generates a `st.global.cs` store instruction.
* \param[out] ptr - memory location
* \param[in] value - the value to be stored
*/
__LWDA_BF16_DECL__ void __stcs(__lw_bfloat16 *const ptr, const __lw_bfloat16 value);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Generates a `st.global.wt` store instruction.
* \param[out] ptr - memory location
* \param[in] value - the value to be stored
*/
__LWDA_BF16_DECL__ void __stwt(__lw_bfloat162 *const ptr, const __lw_bfloat162 value);
/**
* \ingroup LWDA_MATH__BFLOAT16_MISC
* \brief Generates a `st.global.wt` store instruction.
* \param[out] ptr - memory location
* \param[in] value - the value to be stored
*/
__LWDA_BF16_DECL__ void __stwt(__lw_bfloat16 *const ptr, const __lw_bfloat16 value);

/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs lw_bfloat162 vector if-equal comparison.
* 
* \details Performs \p lw_bfloat162 vector if-equal comparison of inputs \p a and \p b.
* The corresponding \p lw_bfloat16 results are set to 1.0 for true, or 0.0 for false.
* NaN inputs generate false results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
* 
* \returns lw_bfloat162
* - The vector result of if-equal comparison of vectors \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __heq2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector not-equal comparison.
* 
* \details Performs \p lw_bfloat162 vector not-equal comparison of inputs \p a and \p b.
* The corresponding \p lw_bfloat16 results are set to 1.0 for true, or 0.0 for false.
* NaN inputs generate false results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
* 
* \returns lw_bfloat162
* - The vector result of not-equal comparison of vectors \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hne2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector less-equal comparison.
*
* \details Performs \p lw_bfloat162 vector less-equal comparison of inputs \p a and \p b.
* The corresponding \p lw_bfloat16 results are set to 1.0 for true, or 0.0 for false.
* NaN inputs generate false results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The \p lw_bfloat162 result of less-equal comparison of vectors \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hle2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector greater-equal comparison.
*
* \details Performs \p lw_bfloat162 vector greater-equal comparison of inputs \p a and \p b.
* The corresponding \p lw_bfloat16 results are set to 1.0 for true, or 0.0 for false.
* NaN inputs generate false results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The vector result of greater-equal comparison of vectors \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hge2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector less-than comparison.
*
* \details Performs \p lw_bfloat162 vector less-than comparison of inputs \p a and \p b.
* The corresponding \p lw_bfloat16 results are set to 1.0 for true, or 0.0 for false.
* NaN inputs generate false results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The lw_bfloat162 vector result of less-than comparison of vectors \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hlt2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector greater-than comparison.
* 
* \details Performs \p lw_bfloat162 vector greater-than comparison of inputs \p a and \p b.
* The corresponding \p lw_bfloat16 results are set to 1.0 for true, or 0.0 for false.
* NaN inputs generate false results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
* 
* \returns lw_bfloat162
* - The vector result of greater-than comparison of vectors \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hgt2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector unordered if-equal comparison.
* 
* \details Performs \p lw_bfloat162 vector if-equal comparison of inputs \p a and \p b.
* The corresponding \p lw_bfloat16 results are set to 1.0 for true, or 0.0 for false.
* NaN inputs generate true results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
* 
* \returns lw_bfloat162
* - The vector result of unordered if-equal comparison of vectors \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hequ2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector unordered not-equal comparison.
*
* \details Performs \p lw_bfloat162 vector not-equal comparison of inputs \p a and \p b.
* The corresponding \p lw_bfloat16 results are set to 1.0 for true, or 0.0 for false.
* NaN inputs generate true results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The vector result of unordered not-equal comparison of vectors \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hneu2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector unordered less-equal comparison.
*
* Performs \p lw_bfloat162 vector less-equal comparison of inputs \p a and \p b.
* The corresponding \p lw_bfloat16 results are set to 1.0 for true, or 0.0 for false.
* NaN inputs generate true results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The vector result of unordered less-equal comparison of vectors \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hleu2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector unordered greater-equal comparison.
*
* \details Performs \p lw_bfloat162 vector greater-equal comparison of inputs \p a and \p b.
* The corresponding \p lw_bfloat16 results are set to 1.0 for true, or 0.0 for false.
* NaN inputs generate true results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The \p lw_bfloat162 vector result of unordered greater-equal comparison of vectors \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hgeu2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector unordered less-than comparison.
*
* \details Performs \p lw_bfloat162 vector less-than comparison of inputs \p a and \p b.
* The corresponding \p lw_bfloat16 results are set to 1.0 for true, or 0.0 for false.
* NaN inputs generate true results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The vector result of unordered less-than comparison of vectors \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hltu2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector unordered greater-than comparison.
*
* \details Performs \p lw_bfloat162 vector greater-than comparison of inputs \p a and \p b.
* The corresponding \p lw_bfloat16 results are set to 1.0 for true, or 0.0 for false.
* NaN inputs generate true results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The \p lw_bfloat162 vector result of unordered greater-than comparison of vectors \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hgtu2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Determine whether \p lw_bfloat162 argument is a NaN.
*
* \details Determine whether each lw_bfloat16 of input \p lw_bfloat162 number \p a is a NaN.
* \param[in] a - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The lw_bfloat162 with the corresponding \p lw_bfloat16 results set to
* 1.0 for NaN, 0.0 otherwise. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hisnan2(const __lw_bfloat162 a);
/**
* \ingroup LWDA_MATH__BFLOAT162_ARITHMETIC
* \brief Performs \p lw_bfloat162 vector addition in round-to-nearest-even mode.
*
* \details Performs \p lw_bfloat162 vector add of inputs \p a and \p b, in round-to-nearest
* mode.
* \internal
* \req DEEPLEARN-SRM_REQ-95
* \endinternal
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The sum of vectors \p a and \p b. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hadd2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_ARITHMETIC
* \brief Performs \p lw_bfloat162 vector subtraction in round-to-nearest-even mode.
*
* \details Subtracts \p lw_bfloat162 input vector \p b from input vector \p a in
* round-to-nearest-even mode.
* \internal
* \req DEEPLEARN-SRM_REQ-104
* \endinternal
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The subtraction of vector \p b from \p a. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hsub2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_ARITHMETIC
* \brief Performs \p lw_bfloat162 vector multiplication in round-to-nearest-even mode.
*
* \details Performs \p lw_bfloat162 vector multiplication of inputs \p a and \p b, in
* round-to-nearest-even mode.
* \internal
* \req DEEPLEARN-SRM_REQ-102
* \endinternal
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The result of elementwise multiplying the vectors \p a and \p b. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hmul2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_ARITHMETIC
* \brief Performs \p lw_bfloat162 vector addition in round-to-nearest-even mode.
*
* \details Performs \p lw_bfloat162 vector add of inputs \p a and \p b, in round-to-nearest
* mode. Prevents floating-point contractions of mul+add into fma.
* \internal
* \req DEEPLEARN-SRM_REQ-95
* \endinternal
* \param[in] a - lw_bfloat162. Is only being read.
* \param[in] b - lw_bfloat162. Is only being read.
*
* \returns lw_bfloat162
* - The sum of vectors \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hadd2_rn(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_ARITHMETIC
* \brief Performs \p lw_bfloat162 vector subtraction in round-to-nearest-even mode.
*
* \details Subtracts \p lw_bfloat162 input vector \p b from input vector \p a in
* round-to-nearest-even mode. Prevents floating-point contractions of mul+sub into fma.
* \internal
* \req DEEPLEARN-SRM_REQ-104
* \endinternal
* \param[in] a - lw_bfloat162. Is only being read.
* \param[in] b - lw_bfloat162. Is only being read.
*
* \returns lw_bfloat162
* - The subtraction of vector \p b from \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hsub2_rn(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_ARITHMETIC
* \brief Performs \p lw_bfloat162 vector multiplication in round-to-nearest-even mode.
*
* \details Performs \p lw_bfloat162 vector multiplication of inputs \p a and \p b, in
* round-to-nearest-even mode. Prevents floating-point contractions of mul+add
* or sub into fma.
* \internal
* \req DEEPLEARN-SRM_REQ-102
* \endinternal
* \param[in] a - lw_bfloat162. Is only being read.
* \param[in] b - lw_bfloat162. Is only being read.
*
* \returns lw_bfloat162
* - The result of elementwise multiplying the vectors \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hmul2_rn(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_ARITHMETIC
* \brief Performs \p lw_bfloat162 vector division in round-to-nearest-even mode.
*
* \details Divides \p lw_bfloat162 input vector \p a by input vector \p b in round-to-nearest
* mode.
* \internal
* \req DEEPLEARN-SRM_REQ-103
* \endinternal
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The elementwise division of \p a with \p b. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __h2div(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_ARITHMETIC
* \brief Callwlates the absolute value of both halves of the input \p lw_bfloat162 number and
* returns the result.
*
* \details Callwlates the absolute value of both halves of the input \p lw_bfloat162 number and
* returns the result.
* \param[in] a - lw_bfloat162. Is only being read. 
*
* \returns bfloat2
* - Returns \p a with the absolute value of both halves. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __habs2(const __lw_bfloat162 a);
/**
* \ingroup LWDA_MATH__BFLOAT162_ARITHMETIC
* \brief Performs \p lw_bfloat162 vector addition in round-to-nearest-even mode, with
* saturation to [0.0, 1.0].
*
* \details Performs \p lw_bfloat162 vector add of inputs \p a and \p b, in round-to-nearest
* mode, and clamps the results to range [0.0, 1.0]. NaN results are flushed to
* +0.0.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The sum of \p a and \p b, with respect to saturation. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hadd2_sat(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_ARITHMETIC
* \brief Performs \p lw_bfloat162 vector subtraction in round-to-nearest-even mode,
* with saturation to [0.0, 1.0].
*
* \details Subtracts \p lw_bfloat162 input vector \p b from input vector \p a in
* round-to-nearest-even mode, and clamps the results to range [0.0, 1.0]. NaN
* results are flushed to +0.0.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The subtraction of vector \p b from \p a, with respect to saturation.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hsub2_sat(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_ARITHMETIC
* \brief Performs \p lw_bfloat162 vector multiplication in round-to-nearest-even mode,
* with saturation to [0.0, 1.0].
*
* \details Performs \p lw_bfloat162 vector multiplication of inputs \p a and \p b, in
* round-to-nearest-even mode, and clamps the results to range [0.0, 1.0]. NaN
* results are flushed to +0.0.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The result of elementwise multiplication of vectors \p a and \p b, 
* with respect to saturation. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hmul2_sat(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_ARITHMETIC
* \brief Performs \p lw_bfloat162 vector fused multiply-add in round-to-nearest-even
* mode.
*
* \details Performs \p lw_bfloat162 vector multiply on inputs \p a and \p b,
* then performs a \p lw_bfloat162 vector add of the result with \p c,
* rounding the result once in round-to-nearest-even mode.
* \internal
* \req DEEPLEARN-SRM_REQ-105
* \endinternal
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
* \param[in] c - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The result of elementwise fused multiply-add operation on vectors \p a, \p b, and \p c. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hfma2(const __lw_bfloat162 a, const __lw_bfloat162 b, const __lw_bfloat162 c);
/**
* \ingroup LWDA_MATH__BFLOAT162_ARITHMETIC
* \brief Performs \p lw_bfloat162 vector fused multiply-add in round-to-nearest-even
* mode, with saturation to [0.0, 1.0].
*
* \details Performs \p lw_bfloat162 vector multiply on inputs \p a and \p b,
* then performs a \p lw_bfloat162 vector add of the result with \p c,
* rounding the result once in round-to-nearest-even mode, and clamps the
* results to range [0.0, 1.0]. NaN results are flushed to +0.0.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
* \param[in] c - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The result of elementwise fused multiply-add operation on vectors \p a, \p b, and \p c, 
* with respect to saturation. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hfma2_sat(const __lw_bfloat162 a, const __lw_bfloat162 b, const __lw_bfloat162 c);
/**
* \ingroup LWDA_MATH__BFLOAT162_ARITHMETIC
* \brief Negates both halves of the input \p lw_bfloat162 number and returns the
* result.
*
* \details Negates both halves of the input \p lw_bfloat162 number \p a and returns the result.
* \internal
* \req DEEPLEARN-SRM_REQ-101
* \endinternal
* \param[in] a - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - Returns \p a with both halves negated. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hneg2(const __lw_bfloat162 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_ARITHMETIC
* \brief Callwlates the absolute value of input \p lw_bfloat16 number and returns the result.
*
* \details Callwlates the absolute value of input \p lw_bfloat16 number and returns the result.
* \param[in] a - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - The absolute value of a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __habs(const __lw_bfloat16 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_ARITHMETIC
* \brief Performs \p lw_bfloat16 addition in round-to-nearest-even mode.
*
* \details Performs \p lw_bfloat16 addition of inputs \p a and \p b, in round-to-nearest-even
* mode.
* \internal
* \req DEEPLEARN-SRM_REQ-94
* \endinternal
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - The sum of \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __hadd(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_ARITHMETIC
* \brief Performs \p lw_bfloat16 subtraction in round-to-nearest-even mode.
*
* \details Subtracts \p lw_bfloat16 input \p b from input \p a in round-to-nearest
* mode.
* \internal
* \req DEEPLEARN-SRM_REQ-97
* \endinternal
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - The result of subtracting \p b from \p a. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __hsub(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_ARITHMETIC
* \brief Performs \p lw_bfloat16 multiplication in round-to-nearest-even mode.
*
* \details Performs \p lw_bfloat16 multiplication of inputs \p a and \p b, in round-to-nearest
* mode.
* \internal
* \req DEEPLEARN-SRM_REQ-99
* \endinternal
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - The result of multiplying \p a and \p b. 
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __hmul(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_ARITHMETIC
* \brief Performs \p lw_bfloat16 addition in round-to-nearest-even mode.
*
* \details Performs \p lw_bfloat16 addition of inputs \p a and \p b, in round-to-nearest-even
* mode. Prevents floating-point contractions of mul+add into fma.
* \internal
* \req DEEPLEARN-SRM_REQ-94
* \endinternal
* \param[in] a - lw_bfloat16. Is only being read.
* \param[in] b - lw_bfloat16. Is only being read.
*
* \returns lw_bfloat16
* - The sum of \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __hadd_rn(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_ARITHMETIC
* \brief Performs \p lw_bfloat16 subtraction in round-to-nearest-even mode.
*
* \details Subtracts \p lw_bfloat16 input \p b from input \p a in round-to-nearest
* mode. Prevents floating-point contractions of mul+sub into fma.
* \internal
* \req DEEPLEARN-SRM_REQ-97
* \endinternal
* \param[in] a - lw_bfloat16. Is only being read.
* \param[in] b - lw_bfloat16. Is only being read.
*
* \returns lw_bfloat16
* - The result of subtracting \p b from \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __hsub_rn(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_ARITHMETIC
* \brief Performs \p lw_bfloat16 multiplication in round-to-nearest-even mode.
*
* \details Performs \p lw_bfloat16 multiplication of inputs \p a and \p b, in round-to-nearest
* mode. Prevents floating-point contractions of mul+add or sub into fma.
* \internal
* \req DEEPLEARN-SRM_REQ-99
* \endinternal
* \param[in] a - lw_bfloat16. Is only being read.
* \param[in] b - lw_bfloat16. Is only being read.
*
* \returns lw_bfloat16
* - The result of multiplying \p a and \p b.
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __hmul_rn(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_ARITHMETIC
* \brief Performs \p lw_bfloat16 division in round-to-nearest-even mode.
* 
* \details Divides \p lw_bfloat16 input \p a by input \p b in round-to-nearest
* mode.
* \internal
* \req DEEPLEARN-SRM_REQ-98
* \endinternal
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
* 
* \returns lw_bfloat16
* - The result of dividing \p a by \p b. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__  __lw_bfloat16 __hdiv(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_ARITHMETIC
* \brief Performs \p lw_bfloat16 addition in round-to-nearest-even mode, with
* saturation to [0.0, 1.0].
*
* \details Performs \p lw_bfloat16 add of inputs \p a and \p b, in round-to-nearest-even mode,
* and clamps the result to range [0.0, 1.0]. NaN results are flushed to +0.0.
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - The sum of \p a and \p b, with respect to saturation.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __hadd_sat(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_ARITHMETIC
* \brief Performs \p lw_bfloat16 subtraction in round-to-nearest-even mode, with
* saturation to [0.0, 1.0].
*
* \details Subtracts \p lw_bfloat16 input \p b from input \p a in round-to-nearest
* mode,
* and clamps the result to range [0.0, 1.0]. NaN results are flushed to +0.0.
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - The result of subtraction of \p b from \p a, with respect to saturation.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __hsub_sat(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_ARITHMETIC
* \brief Performs \p lw_bfloat16 multiplication in round-to-nearest-even mode, with
* saturation to [0.0, 1.0].
*
* \details Performs \p lw_bfloat16 multiplication of inputs \p a and \p b, in round-to-nearest
* mode, and clamps the result to range [0.0, 1.0]. NaN results are flushed to
* +0.0.
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - The result of multiplying \p a and \p b, with respect to saturation.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __hmul_sat(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_ARITHMETIC
* \brief Performs \p lw_bfloat16 fused multiply-add in round-to-nearest-even mode.
*
* \details Performs \p lw_bfloat16 multiply on inputs \p a and \p b,
* then performs a \p lw_bfloat16 add of the result with \p c,
* rounding the result once in round-to-nearest-even mode.
* \internal
* \req DEEPLEARN-SRM_REQ-96
* \endinternal
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
* \param[in] c - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - The result of fused multiply-add operation on \p
* a, \p b, and \p c. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __hfma(const __lw_bfloat16 a, const __lw_bfloat16 b, const __lw_bfloat16 c);
/**
* \ingroup LWDA_MATH__BFLOAT16_ARITHMETIC
* \brief Performs \p lw_bfloat16 fused multiply-add in round-to-nearest-even mode,
* with saturation to [0.0, 1.0].
*
* \details Performs \p lw_bfloat16 multiply on inputs \p a and \p b,
* then performs a \p lw_bfloat16 add of the result with \p c,
* rounding the result once in round-to-nearest-even mode, and clamps the result
* to range [0.0, 1.0]. NaN results are flushed to +0.0.
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
* \param[in] c - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - The result of fused multiply-add operation on \p
* a, \p b, and \p c, with respect to saturation. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __hfma_sat(const __lw_bfloat16 a, const __lw_bfloat16 b, const __lw_bfloat16 c);
/**
* \ingroup LWDA_MATH__BFLOAT16_ARITHMETIC
* \brief Negates input \p lw_bfloat16 number and returns the result.
*
* \details Negates input \p lw_bfloat16 number and returns the result.
* \internal
* \req DEEPLEARN-SRM_REQ-100
* \endinternal
* \param[in] a - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - minus a
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __hneg(const __lw_bfloat16 a);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector if-equal comparison and returns boolean true
* iff both \p lw_bfloat16 results are true, boolean false otherwise.
*
* \details Performs \p lw_bfloat162 vector if-equal comparison of inputs \p a and \p b.
* The bool result is set to true only if both \p lw_bfloat16 if-equal comparisons
* evaluate to true, or false otherwise.
* NaN inputs generate false results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns bool
* - true if both \p lw_bfloat16 results of if-equal comparison
* of vectors \p a and \p b are true;
* - false otherwise.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hbeq2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector not-equal comparison and returns boolean
* true iff both \p lw_bfloat16 results are true, boolean false otherwise.
*
* \details Performs \p lw_bfloat162 vector not-equal comparison of inputs \p a and \p b.
* The bool result is set to true only if both \p lw_bfloat16 not-equal comparisons
* evaluate to true, or false otherwise.
* NaN inputs generate false results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns bool
* - true if both \p lw_bfloat16 results of not-equal comparison
* of vectors \p a and \p b are true, 
* - false otherwise. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hbne2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector less-equal comparison and returns boolean
* true iff both \p lw_bfloat16 results are true, boolean false otherwise.
*
* \details Performs \p lw_bfloat162 vector less-equal comparison of inputs \p a and \p b.
* The bool result is set to true only if both \p lw_bfloat16 less-equal comparisons
* evaluate to true, or false otherwise.
* NaN inputs generate false results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns bool
* - true if both \p lw_bfloat16 results of less-equal comparison
* of vectors \p a and \p b are true; 
* - false otherwise. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hble2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector greater-equal comparison and returns boolean
* true iff both \p lw_bfloat16 results are true, boolean false otherwise.
*
* \details Performs \p lw_bfloat162 vector greater-equal comparison of inputs \p a and \p b.
* The bool result is set to true only if both \p lw_bfloat16 greater-equal comparisons
* evaluate to true, or false otherwise.
* NaN inputs generate false results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns bool
* - true if both \p lw_bfloat16 results of greater-equal
* comparison of vectors \p a and \p b are true; 
* - false otherwise. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hbge2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector less-than comparison and returns boolean
* true iff both \p lw_bfloat16 results are true, boolean false otherwise.
*
* \details Performs \p lw_bfloat162 vector less-than comparison of inputs \p a and \p b.
* The bool result is set to true only if both \p lw_bfloat16 less-than comparisons
* evaluate to true, or false otherwise.
* NaN inputs generate false results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns bool
* - true if both \p lw_bfloat16 results of less-than comparison
* of vectors \p a and \p b are true; 
* - false otherwise. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hblt2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector greater-than comparison and returns boolean
* true iff both \p lw_bfloat16 results are true, boolean false otherwise.
*
* \details Performs \p lw_bfloat162 vector greater-than comparison of inputs \p a and \p b.
* The bool result is set to true only if both \p lw_bfloat16 greater-than comparisons
* evaluate to true, or false otherwise.
* NaN inputs generate false results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
* 
* \returns bool 
* - true if both \p lw_bfloat16 results of greater-than
* comparison of vectors \p a and \p b are true; 
* - false otherwise. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hbgt2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector unordered if-equal comparison and returns
* boolean true iff both \p lw_bfloat16 results are true, boolean false otherwise.
*
* \details Performs \p lw_bfloat162 vector if-equal comparison of inputs \p a and \p b.
* The bool result is set to true only if both \p lw_bfloat16 if-equal comparisons
* evaluate to true, or false otherwise.
* NaN inputs generate true results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns bool
* - true if both \p lw_bfloat16 results of unordered if-equal
* comparison of vectors \p a and \p b are true; 
* - false otherwise. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hbequ2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector unordered not-equal comparison and returns
* boolean true iff both \p lw_bfloat16 results are true, boolean false otherwise.
*
* \details Performs \p lw_bfloat162 vector not-equal comparison of inputs \p a and \p b.
* The bool result is set to true only if both \p lw_bfloat16 not-equal comparisons
* evaluate to true, or false otherwise.
* NaN inputs generate true results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns bool
* - true if both \p lw_bfloat16 results of unordered not-equal
* comparison of vectors \p a and \p b are true;
* - false otherwise. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hbneu2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector unordered less-equal comparison and returns
* boolean true iff both \p lw_bfloat16 results are true, boolean false otherwise.
*
* \details Performs \p lw_bfloat162 vector less-equal comparison of inputs \p a and \p b.
* The bool result is set to true only if both \p lw_bfloat16 less-equal comparisons
* evaluate to true, or false otherwise.
* NaN inputs generate true results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns bool
* - true if both \p lw_bfloat16 results of unordered less-equal
* comparison of vectors \p a and \p b are true; 
* - false otherwise. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hbleu2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector unordered greater-equal comparison and
* returns boolean true iff both \p lw_bfloat16 results are true, boolean false
* otherwise.
*
* \details Performs \p lw_bfloat162 vector greater-equal comparison of inputs \p a and \p b.
* The bool result is set to true only if both \p lw_bfloat16 greater-equal comparisons
* evaluate to true, or false otherwise.
* NaN inputs generate true results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns bool
* - true if both \p lw_bfloat16 results of unordered
* greater-equal comparison of vectors \p a and \p b are true; 
* - false otherwise. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hbgeu2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector unordered less-than comparison and returns
* boolean true iff both \p lw_bfloat16 results are true, boolean false otherwise.
*
* \details Performs \p lw_bfloat162 vector less-than comparison of inputs \p a and \p b.
* The bool result is set to true only if both \p lw_bfloat16 less-than comparisons
* evaluate to true, or false otherwise.
* NaN inputs generate true results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns bool
* - true if both \p lw_bfloat16 results of unordered less-than comparison of 
* vectors \p a and \p b are true; 
* - false otherwise. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hbltu2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Performs \p lw_bfloat162 vector unordered greater-than comparison and
* returns boolean true iff both \p lw_bfloat16 results are true, boolean false
* otherwise.
*
* \details Performs \p lw_bfloat162 vector greater-than comparison of inputs \p a and \p b.
* The bool result is set to true only if both \p lw_bfloat16 greater-than comparisons
* evaluate to true, or false otherwise.
* NaN inputs generate true results.
* \param[in] a - lw_bfloat162. Is only being read. 
* \param[in] b - lw_bfloat162. Is only being read. 
*
* \returns bool
* - true if both \p lw_bfloat16 results of unordered
* greater-than comparison of vectors \p a and \p b are true;
* - false otherwise. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hbgtu2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_COMPARISON
* \brief Performs \p lw_bfloat16 if-equal comparison.
*
* \details Performs \p lw_bfloat16 if-equal comparison of inputs \p a and \p b.
* NaN inputs generate false results.
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
*
* \returns bool
* - The boolean result of if-equal comparison of \p a and \p b. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __heq(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_COMPARISON
* \brief Performs \p lw_bfloat16 not-equal comparison.
*
* \details Performs \p lw_bfloat16 not-equal comparison of inputs \p a and \p b.
* NaN inputs generate false results.
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
*
* \returns bool
* - The boolean result of not-equal comparison of \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hne(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_COMPARISON
* \brief Performs \p lw_bfloat16 less-equal comparison.
*
* \details Performs \p lw_bfloat16 less-equal comparison of inputs \p a and \p b.
* NaN inputs generate false results.
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
*
* \returns bool
* - The boolean result of less-equal comparison of \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hle(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_COMPARISON
* \brief Performs \p lw_bfloat16 greater-equal comparison.
*
* \details Performs \p lw_bfloat16 greater-equal comparison of inputs \p a and \p b.
* NaN inputs generate false results.
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
*
* \returns bool
* - The boolean result of greater-equal comparison of \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hge(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_COMPARISON
* \brief Performs \p lw_bfloat16 less-than comparison.
*
* \details Performs \p lw_bfloat16 less-than comparison of inputs \p a and \p b.
* NaN inputs generate false results.
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
*
* \returns bool
* - The boolean result of less-than comparison of \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hlt(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_COMPARISON
* \brief Performs \p lw_bfloat16 greater-than comparison.
*
* \details Performs \p lw_bfloat16 greater-than comparison of inputs \p a and \p b.
* NaN inputs generate false results.
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
*
* \returns bool
* - The boolean result of greater-than comparison of \p a and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hgt(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_COMPARISON
* \brief Performs \p lw_bfloat16 unordered if-equal comparison.
*
* \details Performs \p lw_bfloat16 if-equal comparison of inputs \p a and \p b.
* NaN inputs generate true results.
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
*
* \returns bool
* - The boolean result of unordered if-equal comparison of \p a and
* \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hequ(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_COMPARISON
* \brief Performs \p lw_bfloat16 unordered not-equal comparison.
*
* \details Performs \p lw_bfloat16 not-equal comparison of inputs \p a and \p b.
* NaN inputs generate true results.
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
*
* \returns bool
* - The boolean result of unordered not-equal comparison of \p a and
* \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hneu(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_COMPARISON
* \brief Performs \p lw_bfloat16 unordered less-equal comparison.
*
* \details Performs \p lw_bfloat16 less-equal comparison of inputs \p a and \p b.
* NaN inputs generate true results.
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
*
* \returns bool
* - The boolean result of unordered less-equal comparison of \p a and
* \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hleu(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_COMPARISON
* \brief Performs \p lw_bfloat16 unordered greater-equal comparison.
*
* \details Performs \p lw_bfloat16 greater-equal comparison of inputs \p a and \p b.
* NaN inputs generate true results.
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
*
* \returns bool
* - The boolean result of unordered greater-equal comparison of \p a
* and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hgeu(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_COMPARISON
* \brief Performs \p lw_bfloat16 unordered less-than comparison.
*
* \details Performs \p lw_bfloat16 less-than comparison of inputs \p a and \p b.
* NaN inputs generate true results.
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
*
* \returns bool
* - The boolean result of unordered less-than comparison of \p a and
* \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hltu(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_COMPARISON
* \brief Performs \p lw_bfloat16 unordered greater-than comparison.
*
* \details Performs \p lw_bfloat16 greater-than comparison of inputs \p a and \p b.
* NaN inputs generate true results.
* \param[in] a - lw_bfloat16. Is only being read. 
* \param[in] b - lw_bfloat16. Is only being read. 
*
* \returns bool
* - The boolean result of unordered greater-than comparison of \p a
* and \p b.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hgtu(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_COMPARISON
* \brief Determine whether \p lw_bfloat16 argument is a NaN.
*
* \details Determine whether \p lw_bfloat16 value \p a is a NaN.
* \param[in] a - lw_bfloat16. Is only being read. 
*
* \returns bool
* - true iff argument is NaN. 
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ bool __hisnan(const __lw_bfloat16 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_COMPARISON
* \brief Callwlates \p lw_bfloat16 maximum of two input values.
*
* \details Callwlates \p lw_bfloat16 max(\p a, \p b)
* defined as (\p a > \p b) ? \p a : \p b. 
* - If either of inputs is NaN, the other input is returned.
* - If both inputs are NaNs, then canonical NaN is returned.
* - If values of both inputs are 0.0, then +0.0 > -0.0
* \param[in] a - lw_bfloat16. Is only being read.
* \param[in] b - lw_bfloat16. Is only being read.
*
* \returns lw_bfloat16
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __hmax(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_COMPARISON
* \brief Callwlates \p lw_bfloat16 minimum of two input values.
*
* \details Callwlates \p lw_bfloat16 min(\p a, \p b)
* defined as (\p a < \p b) ? \p a : \p b.
* - If either of inputs is NaN, the other input is returned.
* - If both inputs are NaNs, then canonical NaN is returned.
* - If values of both inputs are 0.0, then +0.0 > -0.0
* \param[in] a - lw_bfloat16. Is only being read.
* \param[in] b - lw_bfloat16. Is only being read.
*
* \returns lw_bfloat16
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __hmin(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_COMPARISON
* \brief Callwlates \p lw_bfloat16 maximum of two input values, NaNs pass through.
*
* \details Callwlates \p lw_bfloat16 max(\p a, \p b)
* defined as (\p a > \p b) ? \p a : \p b.
* - If either of inputs is NaN, then canonical NaN is returned.
* - If values of both inputs are 0.0, then +0.0 > -0.0
* \param[in] a - lw_bfloat16. Is only being read.
* \param[in] b - lw_bfloat16. Is only being read.
*
* \returns lw_bfloat16
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __hmax_nan(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_COMPARISON
* \brief Callwlates \p lw_bfloat16 minimum of two input values, NaNs pass through.
*
* \details Callwlates \p lw_bfloat16 min(\p a, \p b)
* defined as (\p a < \p b) ? \p a : \p b.
* - If either of inputs is NaN, then canonical NaN is returned.
* - If values of both inputs are 0.0, then +0.0 > -0.0
* \param[in] a - lw_bfloat16. Is only being read.
* \param[in] b - lw_bfloat16. Is only being read.
*
* \returns lw_bfloat16
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __hmin_nan(const __lw_bfloat16 a, const __lw_bfloat16 b);
/**
* \ingroup LWDA_MATH__BFLOAT16_ARITHMETIC
* \brief Performs \p lw_bfloat16 fused multiply-add in round-to-nearest-even mode with relu saturation.
*
* \details Performs \p lw_bfloat16 multiply on inputs \p a and \p b,
* then performs a \p lw_bfloat16 add of the result with \p c,
* rounding the result once in round-to-nearest-even mode.
* Then negative result is clamped to 0.
* NaN result is colwerted to canonical NaN.
* \param[in] a - lw_bfloat16. Is only being read.
* \param[in] b - lw_bfloat16. Is only being read.
* \param[in] c - lw_bfloat16. Is only being read.
*
* \returns lw_bfloat16
* - The result of fused multiply-add operation on \p
* a, \p b, and \p c with relu saturation.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 __hfma_relu(const __lw_bfloat16 a, const __lw_bfloat16 b, const __lw_bfloat16 c);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Callwlates \p lw_bfloat162 vector maximum of two inputs.
*
* \details Callwlates \p lw_bfloat162 vector max(\p a, \p b).
* Elementwise \p lw_bfloat16 operation is defined as
* (\p a > \p b) ? \p a : \p b.
* - If either of inputs is NaN, the other input is returned.
* - If both inputs are NaNs, then canonical NaN is returned.
* - If values of both inputs are 0.0, then +0.0 > -0.0
* \param[in] a - lw_bfloat162. Is only being read.
* \param[in] b - lw_bfloat162. Is only being read.
*
* \returns lw_bfloat162
* - The result of elementwise maximum of vectors \p a  and \p b
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hmax2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Callwlates \p lw_bfloat162 vector minimum of two inputs.
*
* \details Callwlates \p lw_bfloat162 vector min(\p a, \p b).
* Elementwise \p lw_bfloat16 operation is defined as
* (\p a < \p b) ? \p a : \p b.
* - If either of inputs is NaN, the other input is returned.
* - If both inputs are NaNs, then canonical NaN is returned.
* - If values of both inputs are 0.0, then +0.0 > -0.0
* \param[in] a - lw_bfloat162. Is only being read.
* \param[in] b - lw_bfloat162. Is only being read.
*
* \returns lw_bfloat162
* - The result of elementwise minimum of vectors \p a  and \p b
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hmin2(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Callwlates \p lw_bfloat162 vector maximum of two inputs, NaNs pass through.
*
* \details Callwlates \p lw_bfloat162 vector max(\p a, \p b).
* Elementwise \p lw_bfloat16 operation is defined as
* (\p a > \p b) ? \p a : \p b.
* - If either of inputs is NaN, then canonical NaN is returned.
* - If values of both inputs are 0.0, then +0.0 > -0.0
* \param[in] a - lw_bfloat162. Is only being read.
* \param[in] b - lw_bfloat162. Is only being read.
*
* \returns lw_bfloat162
* - The result of elementwise maximum of vectors \p a  and \p b, with NaNs pass through
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hmax2_nan(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_COMPARISON
* \brief Callwlates \p lw_bfloat162 vector minimum of two inputs, NaNs pass through.
*
* \details Callwlates \p lw_bfloat162 vector min(\p a, \p b).
* Elementwise \p lw_bfloat16 operation is defined as
* (\p a < \p b) ? \p a : \p b.
* - If either of inputs is NaN, then canonical NaN is returned.
* - If values of both inputs are 0.0, then +0.0 > -0.0
* \param[in] a - lw_bfloat162. Is only being read.
* \param[in] b - lw_bfloat162. Is only being read.
*
* \returns lw_bfloat162
* - The result of elementwise minimum of vectors \p a  and \p b, with NaNs pass through
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hmin2_nan(const __lw_bfloat162 a, const __lw_bfloat162 b);
/**
* \ingroup LWDA_MATH__BFLOAT162_ARITHMETIC
* \brief Performs \p lw_bfloat162 vector fused multiply-add in round-to-nearest-even
* mode with relu saturation.
*
* \details Performs \p lw_bfloat162 vector multiply on inputs \p a and \p b,
* then performs a \p lw_bfloat162 vector add of the result with \p c,
* rounding the result once in round-to-nearest-even mode.
* Then negative result is clamped to 0.
* NaN result is colwerted to canonical NaN.
* \param[in] a - lw_bfloat162. Is only being read.
* \param[in] b - lw_bfloat162. Is only being read.
* \param[in] c - lw_bfloat162. Is only being read.
*
* \returns lw_bfloat162
* - The result of elementwise fused multiply-add operation on vectors \p a, \p b, and \p c with relu saturation.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hfma2_relu(const __lw_bfloat162 a, const __lw_bfloat162 b, const __lw_bfloat162 c);
/**
* \ingroup LWDA_MATH__BFLOAT162_ARITHMETIC
* \brief Performs fast complex multiply-accumulate
*
* \details Interprets vector \p lw_bfloat162 input pairs \p a, \p b, and \p c as
* complex numbers in \p lw_bfloat16 precision and performs
* complex multiply-accumulate operation: a*b + c
* \param[in] a - lw_bfloat162. Is only being read.
* \param[in] b - lw_bfloat162. Is only being read.
* \param[in] c - lw_bfloat162. Is only being read.
*
* \returns lw_bfloat162
* - The result of complex multiply-accumulate operation on complex numbers \p a, \p b, and \p c
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 __hcmadd(const __lw_bfloat162 a, const __lw_bfloat162 b, const __lw_bfloat162 c);

/**
* \ingroup LWDA_MATH__BFLOAT16_FUNCTIONS
* \brief Callwlates \p lw_bfloat16 square root in round-to-nearest-even mode.
*
* \details Callwlates \p lw_bfloat16 square root of input \p a in round-to-nearest-even mode.
* \param[in] a - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - The square root of \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 hsqrt(const __lw_bfloat16 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_FUNCTIONS
* \brief Callwlates \p lw_bfloat16 reciprocal square root in round-to-nearest-even
* mode.
*
* \details Callwlates \p lw_bfloat16 reciprocal square root of input \p a in round-to-nearest
* mode.
* \param[in] a - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - The reciprocal square root of \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 hrsqrt(const __lw_bfloat16 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_FUNCTIONS
* \brief Callwlates \p lw_bfloat16 reciprocal in round-to-nearest-even mode.
*
* \details Callwlates \p lw_bfloat16 reciprocal of input \p a in round-to-nearest-even mode.
* \param[in] a - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - The reciprocal of \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 hrcp(const __lw_bfloat16 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_FUNCTIONS
* \brief Callwlates \p lw_bfloat16 natural logarithm in round-to-nearest-even mode.
*
* \details Callwlates \p lw_bfloat16 natural logarithm of input \p a in round-to-nearest-even
* mode.
* \param[in] a - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - The natural logarithm of \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 hlog(const __lw_bfloat16 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_FUNCTIONS
* \brief Callwlates \p lw_bfloat16 binary logarithm in round-to-nearest-even mode.
*
* \details Callwlates \p lw_bfloat16 binary logarithm of input \p a in round-to-nearest-even
* mode.
* \param[in] a - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - The binary logarithm of \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 hlog2(const __lw_bfloat16 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_FUNCTIONS
* \brief Callwlates \p lw_bfloat16 decimal logarithm in round-to-nearest-even mode.
*
* \details Callwlates \p lw_bfloat16 decimal logarithm of input \p a in round-to-nearest-even
* mode.
* \param[in] a - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - The decimal logarithm of \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 hlog10(const __lw_bfloat16 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_FUNCTIONS
* \brief Callwlates \p lw_bfloat16 natural exponential function in round-to-nearest
* mode.
*
* \details Callwlates \p lw_bfloat16 natural exponential function of input \p a in
* round-to-nearest-even mode.
* \param[in] a - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - The natural exponential function on \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 hexp(const __lw_bfloat16 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_FUNCTIONS
* \brief Callwlates \p lw_bfloat16 binary exponential function in round-to-nearest
* mode.
*
* \details Callwlates \p lw_bfloat16 binary exponential function of input \p a in
* round-to-nearest-even mode.
* \param[in] a - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - The binary exponential function on \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 hexp2(const __lw_bfloat16 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_FUNCTIONS
* \brief Callwlates \p lw_bfloat16 decimal exponential function in round-to-nearest
* mode.
*
* \details Callwlates \p lw_bfloat16 decimal exponential function of input \p a in
* round-to-nearest-even mode.
* \param[in] a - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - The decimal exponential function on \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 hexp10(const __lw_bfloat16 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_FUNCTIONS
* \brief Callwlates \p lw_bfloat16 cosine in round-to-nearest-even mode.
*
* \details Callwlates \p lw_bfloat16 cosine of input \p a in round-to-nearest-even mode.
* \param[in] a - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - The cosine of \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 hcos(const __lw_bfloat16 a);
/**
* \ingroup LWDA_MATH__BFLOAT16_FUNCTIONS
* \brief Callwlates \p lw_bfloat16 sine in round-to-nearest-even mode.
*
* \details Callwlates \p lw_bfloat16 sine of input \p a in round-to-nearest-even mode.
* \param[in] a - lw_bfloat16. Is only being read. 
*
* \returns lw_bfloat16
* - The sine of \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat16 hsin(const __lw_bfloat16 a);
/**
* \ingroup LWDA_MATH__BFLOAT162_FUNCTIONS
* \brief Callwlates \p lw_bfloat162 vector square root in round-to-nearest-even mode.
*
* \details Callwlates \p lw_bfloat162 square root of input vector \p a in round-to-nearest
* mode.
* \param[in] a - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The elementwise square root on vector \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 h2sqrt(const __lw_bfloat162 a);
/**
* \ingroup LWDA_MATH__BFLOAT162_FUNCTIONS
* \brief Callwlates \p lw_bfloat162 vector reciprocal square root in round-to-nearest
* mode.
*
* \details Callwlates \p lw_bfloat162 reciprocal square root of input vector \p a in
* round-to-nearest-even mode.
* \param[in] a - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The elementwise reciprocal square root on vector \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 h2rsqrt(const __lw_bfloat162 a);
/**
* \ingroup LWDA_MATH__BFLOAT162_FUNCTIONS
* \brief Callwlates \p lw_bfloat162 vector reciprocal in round-to-nearest-even mode.
*
* \details Callwlates \p lw_bfloat162 reciprocal of input vector \p a in round-to-nearest-even
* mode.
* \param[in] a - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The elementwise reciprocal on vector \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 h2rcp(const __lw_bfloat162 a);
/**
* \ingroup LWDA_MATH__BFLOAT162_FUNCTIONS
* \brief Callwlates \p lw_bfloat162 vector natural logarithm in round-to-nearest-even
* mode.
*
* \details Callwlates \p lw_bfloat162 natural logarithm of input vector \p a in
* round-to-nearest-even mode.
* \param[in] a - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The elementwise natural logarithm on vector \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 h2log(const __lw_bfloat162 a);
/**
* \ingroup LWDA_MATH__BFLOAT162_FUNCTIONS
* \brief Callwlates \p lw_bfloat162 vector binary logarithm in round-to-nearest-even
* mode.
*
* \details Callwlates \p lw_bfloat162 binary logarithm of input vector \p a in round-to-nearest
* mode.
* \param[in] a - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The elementwise binary logarithm on vector \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 h2log2(const __lw_bfloat162 a);
/**
* \ingroup LWDA_MATH__BFLOAT162_FUNCTIONS
* \brief Callwlates \p lw_bfloat162 vector decimal logarithm in round-to-nearest-even
* mode.
*
* \details Callwlates \p lw_bfloat162 decimal logarithm of input vector \p a in
* round-to-nearest-even mode.
* \param[in] a - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The elementwise decimal logarithm on vector \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 h2log10(const __lw_bfloat162 a);
/**
* \ingroup LWDA_MATH__BFLOAT162_FUNCTIONS
* \brief Callwlates \p lw_bfloat162 vector exponential function in round-to-nearest
* mode.
*
* \details Callwlates \p lw_bfloat162 exponential function of input vector \p a in
* round-to-nearest-even mode.
* \param[in] a - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The elementwise exponential function on vector \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 h2exp(const __lw_bfloat162 a);
/**
* \ingroup LWDA_MATH__BFLOAT162_FUNCTIONS
* \brief Callwlates \p lw_bfloat162 vector binary exponential function in
* round-to-nearest-even mode.
*
* \details Callwlates \p lw_bfloat162 binary exponential function of input vector \p a in
* round-to-nearest-even mode.
* \param[in] a - lw_bfloat162. Is only being read. 
*
* \returns lw_bfloat162
* - The elementwise binary exponential function on vector \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 h2exp2(const __lw_bfloat162 a);
/**
* \ingroup LWDA_MATH__BFLOAT162_FUNCTIONS
* \brief Callwlates \p lw_bfloat162 vector decimal exponential function in
* round-to-nearest-even mode.
* 
* \details Callwlates \p lw_bfloat162 decimal exponential function of input vector \p a in
* round-to-nearest-even mode.
* \param[in] a - lw_bfloat162. Is only being read. 
* 
* \returns lw_bfloat162
* - The elementwise decimal exponential function on vector \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 h2exp10(const __lw_bfloat162 a);
/**
* \ingroup LWDA_MATH__BFLOAT162_FUNCTIONS
* \brief Callwlates \p lw_bfloat162 vector cosine in round-to-nearest-even mode.
* 
* \details Callwlates \p lw_bfloat162 cosine of input vector \p a in round-to-nearest-even
* mode.
* \param[in] a - lw_bfloat162. Is only being read. 
* 
* \returns lw_bfloat162
* - The elementwise cosine on vector \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 h2cos(const __lw_bfloat162 a);
/**
* \ingroup LWDA_MATH__BFLOAT162_FUNCTIONS
* \brief Callwlates \p lw_bfloat162 vector sine in round-to-nearest-even mode.
* 
* \details Callwlates \p lw_bfloat162 sine of input vector \p a in round-to-nearest-even mode.
* \param[in] a - lw_bfloat162. Is only being read. 
* 
* \returns lw_bfloat162
* - The elementwise sine on vector \p a.
* \internal
* \exception-guarantee no-throw guarantee
* \behavior reentrant, thread safe
* \endinternal
*/
__LWDA_BF16_DECL__ __lw_bfloat162 h2sin(const __lw_bfloat162 a);

/**
* \ingroup LWDA_MATH__BFLOAT162_ARITHMETIC
* \brief Vector add \p val to the value stored at \p address in global or shared memory, and writes this
* value back to \p address. The atomicity of the add operation is guaranteed separately for each of the
* two lw_bfloat16 elements; the entire __lw_bfloat162 is not guaranteed to be atomic as a single 32-bit access.
* 
* \details The location of \p address must be in global or shared memory. This operation has undefined
* behavior otherwise. This operation is only supported by devices of compute capability 8.x and higher.
* 
* \param[in] address - __lw_bfloat162*. An address in global or shared memory.
* \param[in] val - __lw_bfloat162. The value to be added.
* 
* \returns __lw_bfloat162
* - The old value read from \p address.
* 
* \note_ref_guide_atomic
*/
__LWDA_BF16_DECL__ __lw_bfloat162 atomicAdd(__lw_bfloat162 *const address, const __lw_bfloat162 val);

/**
* \ingroup LWDA_MATH__BFLOAT16_ARITHMETIC
* \brief Adds \p val to the value stored at \p address in global or shared memory, and writes this value
* back to \p address. This operation is performed in one atomic operation.
* 
* \details The location of \p address must be in global or shared memory. This operation has undefined
* behavior otherwise. This operation is only supported by devices of compute capability 8.x and higher.
* 
* \param[in] address - __lw_bfloat16*. An address in global or shared memory.
* \param[in] val - __lw_bfloat16. The value to be added.
* 
* \returns __lw_bfloat16
* - The old value read from \p address.
* 
* \note_ref_guide_atomic
*/
__LWDA_BF16_DECL__ __lw_bfloat16 atomicAdd(__lw_bfloat16 *const address, const __lw_bfloat16 val);

#endif /* defined(__LWDACC__) && (__LWDA_ARCH__ >= 800 || !defined(__LWDA_ARCH__)) */

#undef __LWDA_BF16_DECL__
#undef __LWDA_HOSTDEVICE_BF16_DECL__

#endif /* defined(__cplusplus) */

/* Note the .hpp file is included even for host-side compilation, to capture the "lw_bfloat16" & "lw_bfloat162" definitions */
#include "lwda_bf16.hpp"
#undef ___LWDA_BF16_STRINGIFY_INNERMOST
#undef __LWDA_BF16_STRINGIFY

#endif /* end of include guard: __LWDA_BF16_H__ */
