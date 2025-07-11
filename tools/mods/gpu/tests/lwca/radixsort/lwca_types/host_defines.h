/*
 * Copyright 2006-2013 LWPU Corporation.  All rights reserved.
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

#if !defined(__HOST_DEFINES_H__)
#define __HOST_DEFINES_H__

#if defined(__GNUC__) || defined(__LWDA_LIBDEVICE__)

#define __no_return__ \
        __attribute__((noreturn))

#if defined(__LWDACC__) || defined(__LWDA_ARCH__)
/* gcc allows users to define attributes with underscores,
   e.g., __attribute__((__noinline__)).
   Consider a non-LWCA source file (e.g. .cpp) that has the
   above attribute specification, and includes this header file. In that case,
   defining __noinline__ as below  would cause a gcc compilation error.
   Hence, only define __noinline__ when the code is being processed
   by a  LWCA compiler component.
*/
#define __noinline__ \
        __attribute__((noinline))
#endif /* __LWDACC__  || __LWDA_ARCH__ */

#define __forceinline__ \
        __inline__ __attribute__((always_inline))
#define __align__(n) \
        __attribute__((aligned(n)))
#define __thread__ \
        __thread
#define __import__
#define __export__
#define __cdecl
#define __annotate__(a) \
        __attribute__((a))
#define __location__(a) \
        __annotate__(a)
#define LWDARTAPI

#elif defined(_MSC_VER)

#if _MSC_VER >= 1400

#define __restrict__ \
        __restrict

#else /* _MSC_VER >= 1400 */

#define __restrict__

#endif /* _MSC_VER >= 1400 */

#define __inline__ \
        __inline
#define __no_return__ \
        __declspec(noreturn)
#define __noinline__ \
        __declspec(noinline)
#define __forceinline__ \
        __forceinline
#define __align__(n) \
        __declspec(align(n))
#define __thread__ \
        __declspec(thread)
#define __import__ \
        __declspec(dllimport)
#define __export__ \
        __declspec(dllexport)
#define __annotate__(a) \
        __declspec(a)
#define __location__(a) \
        __annotate__(__##a##__)
#define LWDARTAPI \
        __stdcall

#else /* __GNUC__ || __LWDA_LIBDEVICE__ */

#define __inline__

#if !defined(__align__)

#error --- !!! UNKNOWN COMPILER: please provide a LWCA compatible definition for '__align__' !!! ---

#endif /* !__align__ */

#if !defined(LWDARTAPI)

#error --- !!! UNKNOWN COMPILER: please provide a LWCA compatible definition for 'LWDARTAPI' !!! ---

#endif /* !LWDARTAPI */

#endif /* !__GNUC__ */

#if !defined(__GNUC__) || __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 3 && !defined(__clang__) )

#define __specialization_static \
        static

#else /* !__GNUC__ || __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 3) */

#define __specialization_static

#endif /* !__GNUC__ || __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 3) */

#if !defined(__LWDACC__) && !defined(__LWDABE__)

#undef __annotate__
#define __annotate__(a)

#else /* !__LWDACC__ && !__LWDABE__ */

#define __launch_bounds__(...) \
        __annotate__(launch_bounds(__VA_ARGS__))

#endif /* !__LWDACC__ && !__LWDABE__ */

#if defined(__LWDACC__) || defined(__LWDABE__) || \
    defined(__GNUC__) || defined(_WIN64)

#define __builtin_align__(a) \
        __align__(a)

#else /* __LWDACC__ || __LWDABE__ || __GNUC__ || _WIN64 */

#define __builtin_align__(a)

#endif /* __LWDACC__ || __LWDABE__ || __GNUC__  || _WIN64 */

#define __host__ \
        __location__(host)
#define __device__ \
        __location__(device)
#define __global__ \
        __location__(global)
#define __shared__ \
        __location__(shared)
#define __constant__ \
        __location__(constant)
#define __managed__ \
        __location__(managed)

#if defined(__LWDABE__) || !defined(__LWDACC__)
#define __device_builtin__
#define __device_builtin_texture_type__
#define __device_builtin_surface_type__
#define __lwdart_builtin__
#else /* __LWDABE__  || !__LWDACC__ */
#define __device_builtin__ \
        __location__(device_builtin)
#define __device_builtin_texture_type__ \
        __location__(device_builtin_texture_type)
#define __device_builtin_surface_type__ \
        __location__(device_builtin_surface_type)
#define __lwdart_builtin__ \
        __location__(lwdart_builtin)
#endif /* __LWDABE__ || !__LWDACC__ */

#endif /* !__HOST_DEFINES_H__ */
