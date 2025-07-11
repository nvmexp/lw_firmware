/*
 * Copyright 1993-2014 LWPU Corporation.  All rights reserved.
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
 * C.F.R. 35.235 (SEPT 1995) and is provided to the U.S. Government
 * only as a commercial end item.  Consistent with 48 C.F.R.35.235 and
 * 48 C.F.R. 227.7202-1 through 227.7202-4 (JUNE 1995), all
 * U.S. Government End Users acquire the Licensed Deliverables with
 * only those rights set forth herein.
 *
 * Any use of the Licensed Deliverables in individual and commercial
 * software must include, in the user documentation and internal
 * comments to the code, the above Disclaimer and U.S. Government End
 * Users Notice.
 */

#if !defined(__SM_32_ATOMIC_FUNCTIONS_HPP__)
#define __SM_32_ATOMIC_FUNCTIONS_HPP__

#if defined(__LWDACC_RTC__)
#define __SM_32_ATOMIC_FUNCTIONS_DECL__ __device__
#else /* !__LWDACC_RTC__ */
#define __SM_32_ATOMIC_FUNCTIONS_DECL__ static __inline__ __device__
#endif /* __LWDACC_RTC__ */

#if defined(__cplusplus) && defined(__LWDACC__)

#if !defined(__LWDA_ARCH__) || __LWDA_ARCH__ >= 320

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#include "lwda_runtime_api.h"

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

__SM_32_ATOMIC_FUNCTIONS_DECL__ long long atomicMin(long long *address, long long val)
{
    return __illAtomicMin(address, val);
}

__SM_32_ATOMIC_FUNCTIONS_DECL__ long long atomicMax(long long *address, long long val)
{
    return __illAtomicMax(address, val);
}

__SM_32_ATOMIC_FUNCTIONS_DECL__ long long atomicAnd(long long *address, long long val)
{
    return __llAtomicAnd(address, val);
}

__SM_32_ATOMIC_FUNCTIONS_DECL__ long long atomicOr(long long *address, long long val)
{
    return __llAtomicOr(address, val);
}

__SM_32_ATOMIC_FUNCTIONS_DECL__ long long atomicXor(long long *address, long long val)
{
    return __llAtomicXor(address, val);
}

__SM_32_ATOMIC_FUNCTIONS_DECL__ unsigned long long atomicMin(unsigned long long *address, unsigned long long val)
{
    return __ullAtomicMin(address, val);
}

__SM_32_ATOMIC_FUNCTIONS_DECL__ unsigned long long atomicMax(unsigned long long *address, unsigned long long val)
{
    return __ullAtomicMax(address, val);
}

__SM_32_ATOMIC_FUNCTIONS_DECL__ unsigned long long atomicAnd(unsigned long long *address, unsigned long long val)
{
    return __ullAtomicAnd(address, val);
}

__SM_32_ATOMIC_FUNCTIONS_DECL__ unsigned long long atomicOr(unsigned long long *address, unsigned long long val)
{
    return __ullAtomicOr(address, val);
}

__SM_32_ATOMIC_FUNCTIONS_DECL__ unsigned long long atomicXor(unsigned long long *address, unsigned long long val)
{
    return __ullAtomicXor(address, val);
}

#endif /* !__LWDA_ARCH__ || __LWDA_ARCH__ >= 320 */

#endif /* __cplusplus && __LWDACC__ */

#undef __SM_32_ATOMIC_FUNCTIONS_DECL__

#endif /* !__SM_32_ATOMIC_FUNCTIONS_HPP__ */

