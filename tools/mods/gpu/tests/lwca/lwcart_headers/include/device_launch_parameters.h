/*
 * Copyright 1993-2012 LWPU Corporation.  All rights reserved.
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

#if !defined(__DEVICE_LAUNCH_PARAMETERS_H__)
#define __DEVICE_LAUNCH_PARAMETERS_H__

#include "vector_types.h"

#if !defined(__STORAGE__)

#if defined(__LWDACC_RTC__)
#define __STORAGE__ \
        extern const __device__
#else /* !__LWDACC_RTC__ */
#define __STORAGE__ \
        extern const
#endif /* __LWDACC_RTC__ */

#endif /* __STORAGE__ */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

uint3 __device_builtin__ __STORAGE__ threadIdx;
uint3 __device_builtin__ __STORAGE__ blockIdx;
dim3 __device_builtin__ __STORAGE__ blockDim;
dim3 __device_builtin__ __STORAGE__ gridDim;
int __device_builtin__ __STORAGE__ warpSize;

#undef __STORAGE__

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#if !defined(__lwdaGet_threadIdx)

#define __lwdaGet_threadIdx() \
        threadIdx

#endif /* __lwdaGet_threadIdx */

#if !defined(__lwdaGet_blockIdx)

#define __lwdaGet_blockIdx() \
        blockIdx

#endif /* __lwdaGet_blockIdx */

#if !defined(__lwdaGet_blockDim)

#define __lwdaGet_blockDim() \
        blockDim

#endif /* __lwdaGet_blockDim */

#if !defined(__lwdaGet_gridDim)

#define __lwdaGet_gridDim() \
        gridDim

#endif /* __lwdaGet_gridDim */

#if !defined(__lwdaGet_warpSize)

#define __lwdaGet_warpSize() \
        warpSize

#endif /* __lwdaGet_warpSize */

#endif /* !__DEVICE_LAUNCH_PARAMETERS_H__ */
