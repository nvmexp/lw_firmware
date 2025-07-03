/*
 * Copyright 1993-2019 LWPU Corporation.  All rights reserved.
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

#ifndef _LWDA_PIPELINE_PRIMITIVES_H_
# define _LWDA_PIPELINE_PRIMITIVES_H_

# include "lwda_pipeline_helpers.h"

_LWDA_PIPELINE_STATIC_QUALIFIER
void __pipeline_memcpy_async(void* __restrict__ dst_shared, const void* __restrict__ src_global, size_t size_and_align,
                             size_t zfill = 0)
{
    _LWDA_PIPELINE_ASSERT(size_and_align == 4 || size_and_align == 8 || size_and_align == 16);
    _LWDA_PIPELINE_ASSERT(zfill <= size_and_align);
    _LWDA_PIPELINE_ASSERT(__isShared(dst_shared));
    _LWDA_PIPELINE_ASSERT(__isGlobal(src_global));
    _LWDA_PIPELINE_ASSERT(!(reinterpret_cast<uintptr_t>(dst_shared) & (size_and_align - 1)));
    _LWDA_PIPELINE_ASSERT(!(reinterpret_cast<uintptr_t>(src_global) & (size_and_align - 1)));

    switch (size_and_align) {
    case 16:
        switch (zfill) {
        case  0: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async<16, 16>(dst_shared, src_global); return;
        case  1: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async<16, 15>(dst_shared, src_global); return;
        case  2: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async<16, 14>(dst_shared, src_global); return;
        case  3: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async<16, 13>(dst_shared, src_global); return;
        case  4: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async<16, 12>(dst_shared, src_global); return;
        case  5: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async<16, 11>(dst_shared, src_global); return;
        case  6: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async<16, 10>(dst_shared, src_global); return;
        case  7: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async<16,  9>(dst_shared, src_global); return;
        case  8: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async<16,  8>(dst_shared, src_global); return;
        case  9: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async<16,  7>(dst_shared, src_global); return;
        case 10: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async<16,  6>(dst_shared, src_global); return;
        case 11: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async<16,  5>(dst_shared, src_global); return;
        case 12: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async<16,  4>(dst_shared, src_global); return;
        case 13: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async<16,  3>(dst_shared, src_global); return;
        case 14: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async<16,  2>(dst_shared, src_global); return;
        case 15: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async<16,  1>(dst_shared, src_global); return;
        case 16: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async<16,  0>(dst_shared, src_global); return;
        default: _LWDA_PIPELINE_ABORT();                                                                   return;
        }
    case 8:
        switch (zfill) {
        case  0: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async< 8,  8>(dst_shared, src_global); return;
        case  1: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async< 8,  7>(dst_shared, src_global); return;
        case  2: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async< 8,  6>(dst_shared, src_global); return;
        case  3: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async< 8,  5>(dst_shared, src_global); return;
        case  4: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async< 8,  4>(dst_shared, src_global); return;
        case  5: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async< 8,  3>(dst_shared, src_global); return;
        case  6: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async< 8,  2>(dst_shared, src_global); return;
        case  7: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async< 8,  1>(dst_shared, src_global); return;
        case  8: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async< 8,  0>(dst_shared, src_global); return;
        default: _LWDA_PIPELINE_ABORT();                                                                   return;
        }
    case 4:
        switch (zfill) {
        case  0: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async< 4,  4>(dst_shared, src_global); return;
        case  1: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async< 4,  3>(dst_shared, src_global); return;
        case  2: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async< 4,  2>(dst_shared, src_global); return;
        case  3: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async< 4,  1>(dst_shared, src_global); return;
        case  4: _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_memcpy_async< 4,  0>(dst_shared, src_global); return;
        default: _LWDA_PIPELINE_ABORT();                                                                   return;
        }
    default:
        _LWDA_PIPELINE_ABORT();
        return;
    }
}

_LWDA_PIPELINE_STATIC_QUALIFIER
void __pipeline_commit()
{
    _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_commit();
}

_LWDA_PIPELINE_STATIC_QUALIFIER
void __pipeline_wait_prior(size_t prior)
{
    switch (prior) {
    case  0 : _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_wait_prior<0>(); return;
    case  1 : _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_wait_prior<1>(); return;
    case  2 : _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_wait_prior<2>(); return;
    case  3 : _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_wait_prior<3>(); return;
    case  4 : _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_wait_prior<4>(); return;
    case  5 : _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_wait_prior<5>(); return;
    case  6 : _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_wait_prior<6>(); return;
    case  7 : _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_wait_prior<7>(); return;
    default : _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_wait_prior<8>(); return;
    }
}

# if defined(_LWDA_PIPELINE_ARCH_700_OR_LATER)
#  include "lwda_awbarrier_primitives.h"

_LWDA_PIPELINE_STATIC_QUALIFIER
void __pipeline_arrive_on(__mbarrier_t* barrier)
{
    _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_arrive_on(barrier);
}
# endif

#endif /* !_LWDA_PIPELINE_PRIMITIVES_H_ */
