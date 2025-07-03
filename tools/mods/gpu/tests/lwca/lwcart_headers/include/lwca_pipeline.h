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

#ifndef _LWDA_PIPELINE_H_
# define _LWDA_PIPELINE_H_

# include "lwda_pipeline_primitives.h"

# if !defined(_LWDA_PIPELINE_CPLUSPLUS_11_OR_LATER)
#  error This file requires compiler support for the ISO C++ 2011 standard. This support must be enabled with the \
         -std=c++11 compiler option.
# endif

# if defined(_LWDA_PIPELINE_ARCH_700_OR_LATER)
#  include "lwda_awbarrier.h"
# endif

// Integration with liblw++'s lwca::barrier<lwca::thread_scope_block>.

# if defined(_LWDA_PIPELINE_ARCH_700_OR_LATER)
#  if defined(_LIBLWDACXX_LWDA_ABI_VERSION)
#   define _LIBLWDACXX_PIPELINE_ASSUMED_ABI_VERSION _LIBLWDACXX_LWDA_ABI_VERSION
#  else
#   define _LIBLWDACXX_PIPELINE_ASSUMED_ABI_VERSION 4
#  endif

#  define _LIBLWDACXX_PIPELINE_CONCAT(X, Y) X ## Y
#  define _LIBLWDACXX_PIPELINE_CONCAT2(X, Y) _LIBLWDACXX_PIPELINE_CONCAT(X, Y)
#  define _LIBLWDACXX_PIPELINE_INLINE_NAMESPACE _LIBLWDACXX_PIPELINE_CONCAT2(__, _LIBLWDACXX_PIPELINE_ASSUMED_ABI_VERSION)

namespace lwca { inline namespace _LIBLWDACXX_PIPELINE_INLINE_NAMESPACE {
    struct __block_scope_barrier_base;
}}

# endif

_LWDA_PIPELINE_BEGIN_NAMESPACE

template<size_t N, typename T>
_LWDA_PIPELINE_QUALIFIER
auto segment(T* ptr) -> T(*)[N];

class pipeline {
public:
    pipeline(const pipeline&) = delete;
    pipeline(pipeline&&) = delete;
    pipeline& operator=(const pipeline&) = delete;
    pipeline& operator=(pipeline&&) = delete;

    _LWDA_PIPELINE_QUALIFIER pipeline();
    _LWDA_PIPELINE_QUALIFIER size_t commit();
    _LWDA_PIPELINE_QUALIFIER void commit_and_wait();
    _LWDA_PIPELINE_QUALIFIER void wait(size_t batch);
    template<unsigned N>
    _LWDA_PIPELINE_QUALIFIER void wait_prior();

# if defined(_LWDA_PIPELINE_ARCH_700_OR_LATER)
    _LWDA_PIPELINE_QUALIFIER void arrive_on(awbarrier& barrier);
    _LWDA_PIPELINE_QUALIFIER void arrive_on(lwca::__block_scope_barrier_base& barrier);
# endif

private:
    size_t lwrrent_batch;
};

template<class T>
_LWDA_PIPELINE_QUALIFIER
void memcpy_async(T& dst, const T& src, pipeline& pipe);

template<class T, size_t DstN, size_t SrcN>
_LWDA_PIPELINE_QUALIFIER
void memcpy_async(T(*dst)[DstN], const T(*src)[SrcN], pipeline& pipe);

template<size_t N, typename T>
_LWDA_PIPELINE_QUALIFIER
auto segment(T* ptr) -> T(*)[N]
{
    return (T(*)[N])ptr;
}

_LWDA_PIPELINE_QUALIFIER
pipeline::pipeline()
    : lwrrent_batch(0)
{
}

_LWDA_PIPELINE_QUALIFIER
size_t pipeline::commit()
{
    _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_commit();
    return this->lwrrent_batch++;
}

_LWDA_PIPELINE_QUALIFIER
void pipeline::commit_and_wait()
{
    (void)pipeline::commit();
    pipeline::wait_prior<0>();
}

_LWDA_PIPELINE_QUALIFIER
void pipeline::wait(size_t batch)
{
    const size_t prior = this->lwrrent_batch > batch ? this->lwrrent_batch - batch : 0;

    switch (prior) {
    case  0 : _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_wait_prior<0>(); break;
    case  1 : _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_wait_prior<1>(); break;
    case  2 : _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_wait_prior<2>(); break;
    case  3 : _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_wait_prior<3>(); break;
    case  4 : _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_wait_prior<4>(); break;
    case  5 : _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_wait_prior<5>(); break;
    case  6 : _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_wait_prior<6>(); break;
    case  7 : _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_wait_prior<7>(); break;
    default : _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_wait_prior<8>(); break;
    }
}

template<unsigned N>
_LWDA_PIPELINE_QUALIFIER
void pipeline::wait_prior()
{
    _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_wait_prior<N>();
}

# if defined(_LWDA_PIPELINE_ARCH_700_OR_LATER)
_LWDA_PIPELINE_QUALIFIER
void pipeline::arrive_on(awbarrier& barrier)
{
    _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_arrive_on(&barrier.barrier);
}

_LWDA_PIPELINE_QUALIFIER
void pipeline::arrive_on(lwca::__block_scope_barrier_base & barrier)
{
    _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_arrive_on(reinterpret_cast<uint64_t *>(&barrier));
}
# endif

template<class T>
_LWDA_PIPELINE_QUALIFIER
void memcpy_async(T& dst, const T& src, pipeline& pipe)
{
    _LWDA_PIPELINE_ASSERT(!(reinterpret_cast<uintptr_t>(&src) & (alignof(T) - 1)));
    _LWDA_PIPELINE_ASSERT(!(reinterpret_cast<uintptr_t>(&dst) & (alignof(T) - 1)));

    if (__is_trivially_copyable(T)) {
        _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_copy_relaxed<sizeof(T), alignof(T)>(
                reinterpret_cast<void*>(&dst), reinterpret_cast<const void*>(&src));
    } else {
        dst = src;
    }
}

template<class T, size_t DstN, size_t SrcN>
_LWDA_PIPELINE_QUALIFIER
void memcpy_async(T(*dst)[DstN], const T(*src)[SrcN], pipeline& pipe)
{
    constexpr size_t dst_size = sizeof(*dst);
    constexpr size_t src_size = sizeof(*src);
    static_assert(dst_size == 4 || dst_size == 8 || dst_size == 16, "Unsupported copy size.");
    static_assert(src_size <= dst_size, "Source size must be less than or equal to destination size.");
    _LWDA_PIPELINE_ASSERT(!(reinterpret_cast<uintptr_t>(src) & (dst_size - 1)));
    _LWDA_PIPELINE_ASSERT(!(reinterpret_cast<uintptr_t>(dst) & (dst_size - 1)));

    if (__is_trivially_copyable(T)) {
        _LWDA_PIPELINE_INTERNAL_NAMESPACE::pipeline_copy_strict<sizeof(*dst), sizeof(*src)>(
                reinterpret_cast<void*>(*dst), reinterpret_cast<const void*>(*src));
    } else {
        for (size_t i = 0; i < DstN; ++i) {
            (*dst)[i] = (i < SrcN) ? (*src)[i] : T();
        }
    }
}

_LWDA_PIPELINE_END_NAMESPACE

#endif /* !_LWDA_PIPELINE_H_ */
