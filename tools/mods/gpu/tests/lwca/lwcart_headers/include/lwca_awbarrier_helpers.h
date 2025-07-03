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

#ifndef _LWDA_AWBARRIER_HELPERS_H_
#define _LWDA_AWBARRIER_HELPERS_H_

#define _LWDA_AWBARRIER_NAMESPACE       lwlwda::experimental
#define _LWDA_AWBARRIER_BEGIN_NAMESPACE namespace lwlwda { namespace experimental {
#define _LWDA_AWBARRIER_END_NAMESPACE   } }

#define _LWDA_AWBARRIER_INTERNAL_NAMESPACE       _LWDA_AWBARRIER_NAMESPACE::__awbarrier_internal
#define _LWDA_AWBARRIER_BEGIN_INTERNAL_NAMESPACE _LWDA_AWBARRIER_BEGIN_NAMESPACE namespace __awbarrier_internal {
#define _LWDA_AWBARRIER_END_INTERNAL_NAMESPACE   } _LWDA_AWBARRIER_END_NAMESPACE

# if !defined(_LWDA_AWBARRIER_QUALIFIER)
#  define _LWDA_AWBARRIER_QUALIFIER inline __device__
# endif
# if !defined(_LWDA_AWBARRIER_STATIC_QUALIFIER)
#  define _LWDA_AWBARRIER_STATIC_QUALIFIER static inline __device__
#endif

#if defined(__LWDA_ARCH__)
#if (__LWDA_ARCH__ >= 900)
# define _LWDA_AWBARRIER_SM_TARGET _LWDA_AWBARRIER_SM_90
#elif  (__LWDA_ARCH__ >= 800)
# define _LWDA_AWBARRIER_SM_TARGET _LWDA_AWBARRIER_SM_80
#elif (__LWDA_ARCH__ >= 700)
# define _LWDA_AWBARRIER_SM_TARGET _LWDA_AWBARRIER_SM_70
#endif
#else
# define _LWDA_AWBARRIER_SM_TARGET _LWDA_AWBARRIER_SM_70
#endif

#define _LWDA_AWBARRIER_MAX_COUNT ((1 << 14) - 1)

#if defined(__cplusplus) && ((__cplusplus >= 201103L) || (defined(_MSC_VER) && (_MSC_VER >= 1900)))
# define _LWDA_AWBARRIER_CPLUSPLUS_11_OR_LATER
#endif

#if !defined(_LWDA_AWBARRIER_DEBUG)
# if defined(__LWDACC_DEBUG__)
#  define _LWDA_AWBARRIER_DEBUG 1
# else
#  define _LWDA_AWBARRIER_DEBUG 0
# endif
#endif

#if defined(_LWDA_AWBARRIER_DEBUG) && (_LWDA_AWBARRIER_DEBUG == 1) && !defined(NDEBUG)
# if !defined(__LWDACC_RTC__)
#  include <cassert>
# endif
# define _LWDA_AWBARRIER_ASSERT(x) assert((x));
# define _LWDA_AWBARRIER_ABORT() assert(0);
#else
# define _LWDA_AWBARRIER_ASSERT(x)
# define _LWDA_AWBARRIER_ABORT() __trap();
#endif

#if defined(__LWDACC_RTC__)
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef uint64_t           uintptr_t;
#else
# include <stdint.h>
#endif

#if defined(_LWDA_AWBARRIER_SM_TARGET)

typedef uint64_t __mbarrier_t;
typedef uint64_t __mbarrier_token_t;

_LWDA_AWBARRIER_BEGIN_INTERNAL_NAMESPACE

extern "C" __device__ uint32_t __lwvm_get_smem_pointer(void *);

namespace _LWDA_AWBARRIER_SM_70 {
    union AWBarrier {
        struct {
            uint32_t expected;
            uint32_t pending;
        } split;
        uint64_t raw;
    };

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    void __awbarrier_init(uint64_t* barrier, uint32_t expected_count) {
        _LWDA_AWBARRIER_ASSERT(__isShared(barrier));
        _LWDA_AWBARRIER_ASSERT(expected_count > 0 && expected_count < (1 << 29));

        AWBarrier* awbarrier = reinterpret_cast<AWBarrier*>(barrier);

        awbarrier->split.expected = 0x40000000 - expected_count;
        awbarrier->split.pending = 0x80000000 - expected_count;
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    void __awbarrier_ilwal(uint64_t* barrier) {
        _LWDA_AWBARRIER_ASSERT(__isShared(barrier));
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    uint32_t __awbarrier_token_pending_count(uint64_t token) {
        const uint32_t pending = token >> 32;
        return 0x80000000 - (pending & 0x7fffffff);
    }

    template<bool _Drop>
    _LWDA_AWBARRIER_STATIC_QUALIFIER
    uint64_t __awbarrier_arrive_drop(uint64_t* barrier) {
        _LWDA_AWBARRIER_ASSERT(__isShared(barrier));

        AWBarrier* awbarrier = reinterpret_cast<AWBarrier*>(barrier);

        while ((*reinterpret_cast<volatile uint32_t*>(&awbarrier->split.pending) & 0x7fffffff) == 0);

        if (_Drop) {
            (void)atomicAdd_block(&awbarrier->split.expected, 1);
        }

        __threadfence_block();

        const uint32_t old_pending = atomicAdd_block(&awbarrier->split.pending, 1);
        const uint32_t new_pending = old_pending + 1;
        const bool reset = (old_pending ^ new_pending) & 0x80000000;

        if (reset) {
            __threadfence_block();

            uint32_t new_expected = *reinterpret_cast<volatile uint32_t*>(&awbarrier->split.expected);
            new_expected &= ~0x40000000;
            if (new_expected & 0x20000000) {
                new_expected |= 0x40000000;
            }
            atomicAdd_block(&awbarrier->split.pending, new_expected);
        }

        return static_cast<uint64_t>(old_pending) << 32;
    }

    template<bool _Drop>
    _LWDA_AWBARRIER_STATIC_QUALIFIER
    uint64_t __awbarrier_arrive_drop_no_complete(uint64_t* barrier, uint32_t count) {
        _LWDA_AWBARRIER_ASSERT(__isShared(barrier));
        _LWDA_AWBARRIER_ASSERT(count > 0 && count < (1 << 29));

        AWBarrier* awbarrier = reinterpret_cast<AWBarrier*>(barrier);

        while ((*reinterpret_cast<volatile uint32_t*>(&awbarrier->split.pending) & 0x7fffffff) == 0);

        if (_Drop) {
            (void)atomicAdd_block(&awbarrier->split.expected, count);
        }

        return static_cast<uint64_t>(atomicAdd_block(&awbarrier->split.pending, count)) << 32;
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    bool __awbarrier_test_wait(uint64_t* barrier, uint64_t token) {
        _LWDA_AWBARRIER_ASSERT(__isShared(barrier));

        volatile AWBarrier* awbarrier = reinterpret_cast<volatile AWBarrier*>(barrier);

        return ((token >> 32) ^ awbarrier->split.pending) & 0x80000000;
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    uint64_t __awbarrier_arrive_tx(uint64_t* barrier, uint32_t arrive_count, uint32_t tx_count) {
        _LWDA_AWBARRIER_ABORT()
        return 0;
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    void __awbarrier_notoken_arrive_tx(__mbarrier_t* barrier, uint32_t arrive_count, uint32_t tx_count) {
        _LWDA_AWBARRIER_ABORT()
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    bool __awbarrier_test_wait_parity(uint64_t* barrier, bool phase_parity) {
        _LWDA_AWBARRIER_ABORT()
        return false;
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    bool __awbarrier_try_wait(uint64_t* barrier, uint64_t token, uint32_t max_sleep_nanosec) {
        _LWDA_AWBARRIER_ABORT()
        return false;
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    bool __awbarrier_try_wait_parity(uint64_t* barrier, bool phase_parity, uint32_t max_sleep_nanosec) {
        _LWDA_AWBARRIER_ABORT()
        return false;
    }
}; // namespace _LWDA_AWBARRIER_SM_70

namespace _LWDA_AWBARRIER_SM_80 {
    _LWDA_AWBARRIER_STATIC_QUALIFIER
    void __awbarrier_init(uint64_t* barrier, uint32_t expected_count) {
        _LWDA_AWBARRIER_ASSERT(__isShared(barrier));
        _LWDA_AWBARRIER_ASSERT(expected_count > 0 && expected_count < (1 << 29));

        asm volatile ("mbarrier.init.shared.b64 [%0], %1;"
                :
                : "r"(__lwvm_get_smem_pointer(barrier)), "r"(expected_count)
                : "memory");
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    void __awbarrier_ilwal(uint64_t* barrier) {
        _LWDA_AWBARRIER_ASSERT(__isShared(barrier));

        asm volatile ("mbarrier.ilwal.shared.b64 [%0];"
                :
                : "r"(__lwvm_get_smem_pointer(barrier))
                : "memory");
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    uint32_t __awbarrier_token_pending_count(uint64_t token) {
        uint32_t __pending_count;

        asm ("mbarrier.pending_count.b64 %0, %1;"
                : "=r"(__pending_count)
                : "l"(token));
        return __pending_count;
    }

    template<bool _Drop>
    _LWDA_AWBARRIER_STATIC_QUALIFIER
    uint64_t __awbarrier_arrive_drop(uint64_t* barrier) {
        _LWDA_AWBARRIER_ASSERT(__isShared(barrier));

        uint64_t token;

        if (_Drop) {
            asm volatile ("mbarrier.arrive_drop.shared.b64 %0, [%1];"
                    : "=l"(token)
                    : "r"(__lwvm_get_smem_pointer(barrier))
                    : "memory");
        } else {
            asm volatile ("mbarrier.arrive.shared.b64 %0, [%1];"
                    : "=l"(token)
                    : "r"(__lwvm_get_smem_pointer(barrier))
                    : "memory");
        }

        return token;
    }

    template<bool _Drop>
    _LWDA_AWBARRIER_STATIC_QUALIFIER
    uint64_t __awbarrier_arrive_drop_no_complete(uint64_t* barrier, uint32_t count) {
        _LWDA_AWBARRIER_ASSERT(__isShared(barrier));
        _LWDA_AWBARRIER_ASSERT(count > 0 && count < (1 << 29));

        uint64_t token;

        if (_Drop) {
            asm volatile ("mbarrier.arrive_drop.noComplete.shared.b64 %0, [%1], %2;"
                    : "=l"(token)
                    : "r"(__lwvm_get_smem_pointer(barrier)), "r"(count)
                    : "memory");
        } else {
            asm volatile ("mbarrier.arrive.noComplete.shared.b64 %0, [%1], %2;"
                    : "=l"(token)
                    : "r"(__lwvm_get_smem_pointer(barrier)), "r"(count)
                    : "memory");
        }

        return token;
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    bool __awbarrier_test_wait(uint64_t* barrier, uint64_t token) {
        _LWDA_AWBARRIER_ASSERT(__isShared(barrier));

        uint16_t __wait_complete;

        asm volatile ("{"
                "    .reg .pred %%p;"
                "    mbarrier.test_wait.shared.b64 %%p, [%1], %2;"
                "    selp.u16 %0, 1, 0, %%p;"
                "}"
                : "=h"(__wait_complete)
                : "r"(__lwvm_get_smem_pointer(barrier)), "l"(token)
                : "memory");
        return bool(__wait_complete);
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    uint64_t __awbarrier_arrive_tx(uint64_t* barrier, uint32_t arrive_count, uint32_t tx_count) {
        _LWDA_AWBARRIER_ABORT()
        return 0;
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    void __awbarrier_notoken_arrive_tx(__mbarrier_t* barrier, uint32_t arrive_count, uint32_t tx_count) {
        _LWDA_AWBARRIER_ABORT()
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    bool __awbarrier_test_wait_parity(uint64_t* barrier, bool phase_parity) {
        _LWDA_AWBARRIER_ABORT()
        return false;
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    bool __awbarrier_try_wait( uint64_t* barrier, uint64_t token, uint32_t max_sleep_nanosec) {
        _LWDA_AWBARRIER_ABORT()
        return false;
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    bool __awbarrier_try_wait_parity(uint64_t* barrier, bool phase_parity, uint32_t max_sleep_nanosec) {
        _LWDA_AWBARRIER_ABORT()
        return false;
    }
}; // namespace _LWDA_AWBARRIER_SM_80

namespace _LWDA_AWBARRIER_SM_90 {

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    void __awbarrier_init(uint64_t* barrier, uint32_t expected_count) {
        _LWDA_AWBARRIER_SM_80::__awbarrier_init(barrier, expected_count);
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    void __awbarrier_ilwal(uint64_t* barrier) {
        _LWDA_AWBARRIER_SM_80::__awbarrier_ilwal(barrier);
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    uint32_t __awbarrier_token_pending_count(uint64_t token) {
        return _LWDA_AWBARRIER_SM_80::__awbarrier_token_pending_count(token);
    }

    template<bool _Drop>
    _LWDA_AWBARRIER_STATIC_QUALIFIER
    uint64_t __awbarrier_arrive_drop(uint64_t* barrier) {
        return _LWDA_AWBARRIER_SM_80::__awbarrier_arrive_drop<_Drop>(barrier);
    }

    template<bool _Drop>
    _LWDA_AWBARRIER_STATIC_QUALIFIER
    uint64_t __awbarrier_arrive_drop_no_complete(uint64_t* barrier, uint32_t count) {
        return _LWDA_AWBARRIER_SM_80::__awbarrier_arrive_drop_no_complete<_Drop>(barrier, count);
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    bool __awbarrier_test_wait(uint64_t* barrier, uint64_t token) {
        return _LWDA_AWBARRIER_SM_80::__awbarrier_test_wait(barrier, token);
    }
    _LWDA_AWBARRIER_STATIC_QUALIFIER
    uint64_t __awbarrier_arrive_tx(uint64_t* barrier, uint32_t arrive_count, uint32_t tx_count) {
        _LWDA_AWBARRIER_ASSERT(__isShared(barrier));
        uint64_t token;

        if (0 == tx_count) {
            asm volatile ("mbarrier.arrive.shared.b64 %0, [%1], %2;"
                    : "=l"(token)
                    : "r"(__lwvm_get_smem_pointer(barrier)), "r"(arrive_count)
                    : "memory");
        } else if (1 == arrive_count) {
            asm volatile ("mbarrier.arrive.expect_tx.shared.b64 %0, [%1], %2;"
                    : "=l"(token)
                    : "r"(__lwvm_get_smem_pointer(barrier)), "r"(tx_count)
                    : "memory");
        } else {
            asm volatile ("mbarrier.expect_tx.shared.b64 [%0], %1;"
                    :: "r"(__lwvm_get_smem_pointer(barrier)), "r"(tx_count)
                    : "memory");
            asm volatile ("mbarrier.arrive.shared.b64 %0, [%1], %2;"
                    : "=l"(token)
                    : "r"(__lwvm_get_smem_pointer(barrier)), "r"(tx_count)
                    : "memory");
        }

        return token;
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    void __awbarrier_notoken_arrive_tx(__mbarrier_t* barrier, uint32_t arrive_count, uint32_t tx_count) {
        _LWDA_AWBARRIER_ASSERT(__isShared(barrier));

        if (0 == tx_count) {
            asm volatile ("mbarrier.arrive.shared.b64 [%1], %2;"
                    :: "r"(__lwvm_get_smem_pointer(barrier)), "r"(arrive_count)
                    : "memory");
        } else if (1 == arrive_count) {
            asm volatile ("mbarrier.arrive.expect_tx.shared.b64 [%1], %2;"
                    :: "r"(__lwvm_get_smem_pointer(barrier)), "r"(tx_count)
                    : "memory");
        } else {
            asm volatile ("mbarrier.expect_tx.shared.b64 [%0], %1;"
                    :: "r"(__lwvm_get_smem_pointer(barrier)), "r"(tx_count)
                    : "memory");
            asm volatile ("mbarrier.arrive.shared.b64 [%1], %2;"
                    :: "r"(__lwvm_get_smem_pointer(barrier)), "r"(tx_count)
                    : "memory");
        }
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    bool __awbarrier_test_wait_parity(uint64_t* barrier, bool phase_parity) {
        _LWDA_AWBARRIER_ASSERT(__isShared(barrier));
        uint32_t __wait_complete = 0;

        asm volatile ("mbarrier.test_wait.parity.shared.b64 %0, [%1], %2;"
                : "=r"(__wait_complete)
                : "r"(__lwvm_get_smem_pointer(barrier)), "r"(static_cast<uint32_t>(phase_parity))
                : "memory");

        return __wait_complete != 0;
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    bool __awbarrier_try_wait(uint64_t* barrier, uint64_t token, uint32_t max_sleep_nanosec) {
        _LWDA_AWBARRIER_ASSERT(__isShared(barrier));
        uint32_t __wait_complete = 0;

        asm volatile ("mbarrier.try_wait.shared.b64 %0, [%1], %2, %3;"
                : "=r"(__wait_complete)
                : "r"(__lwvm_get_smem_pointer(barrier)), "l"(token), "r"(max_sleep_nanosec)
                : "memory");

        return __wait_complete != 0;
    }

    _LWDA_AWBARRIER_STATIC_QUALIFIER
    bool __awbarrier_try_wait_parity(uint64_t* barrier, bool phase_parity, uint32_t max_sleep_nanosec) {
        _LWDA_AWBARRIER_ASSERT(__isShared(barrier));
        uint32_t __wait_complete = 0;

        asm volatile ("mbarrier.try_wait.parity.shared.b64 %0, [%1], %2, %3;"
                : "=r"(__wait_complete)
                : "r"(__lwvm_get_smem_pointer(barrier)), "r"(static_cast<uint32_t>(phase_parity)), "r"(max_sleep_nanosec)
                : "memory");

        return __wait_complete != 0;
    }
}; // namespace _LWDA_AWBARRIER_SM_90

_LWDA_AWBARRIER_QUALIFIER
void awbarrier_init(uint64_t* barrier, uint32_t expected_count)
{
    _LWDA_AWBARRIER_SM_TARGET::__awbarrier_init(barrier, expected_count);
}

_LWDA_AWBARRIER_QUALIFIER
void awbarrier_ilwal(uint64_t* barrier)
{
    _LWDA_AWBARRIER_SM_TARGET::__awbarrier_ilwal(barrier);
}

_LWDA_AWBARRIER_QUALIFIER
uint32_t awbarrier_token_pending_count(uint64_t token)
{
    return _LWDA_AWBARRIER_SM_TARGET::__awbarrier_token_pending_count(token);
}

template<bool _Drop>
_LWDA_AWBARRIER_QUALIFIER
uint64_t awbarrier_arrive_drop_no_complete(uint64_t* barrier, uint32_t arrive_count)
{
    return _LWDA_AWBARRIER_SM_TARGET::__awbarrier_arrive_drop_no_complete<_Drop>(barrier, arrive_count);
}

template<bool _Drop>
_LWDA_AWBARRIER_QUALIFIER
uint64_t awbarrier_arrive_drop(uint64_t* barrier)
{
    return _LWDA_AWBARRIER_SM_TARGET::__awbarrier_arrive_drop<_Drop>(barrier);
}

_LWDA_AWBARRIER_QUALIFIER
bool awbarrier_test_wait(uint64_t* barrier, uint64_t token)
{
    return _LWDA_AWBARRIER_SM_TARGET::__awbarrier_test_wait(barrier, token);
}

_LWDA_AWBARRIER_QUALIFIER
uint64_t awbarrier_arrive_tx(uint64_t* barrier, uint32_t arrive_count, uint32_t tx_count) {
    return _LWDA_AWBARRIER_SM_TARGET::__awbarrier_arrive_tx(barrier, arrive_count, tx_count);
}

_LWDA_AWBARRIER_QUALIFIER
void awbarrier_notoken_arrive_tx(__mbarrier_t* barrier, uint32_t arrive_count, uint32_t tx_count) {
    _LWDA_AWBARRIER_SM_TARGET::__awbarrier_notoken_arrive_tx(barrier, arrive_count, tx_count);
}

_LWDA_AWBARRIER_QUALIFIER
bool awbarrier_test_wait_parity(uint64_t* barrier, bool phase_parity) {
    return _LWDA_AWBARRIER_SM_TARGET::__awbarrier_test_wait_parity(barrier, phase_parity);
}

_LWDA_AWBARRIER_QUALIFIER
bool awbarrier_try_wait( uint64_t* barrier, uint64_t token, uint32_t max_sleep_nanosec) {
    return _LWDA_AWBARRIER_SM_TARGET::__awbarrier_try_wait(barrier, token, max_sleep_nanosec);
}

_LWDA_AWBARRIER_QUALIFIER
bool awbarrier_try_wait_parity(uint64_t* barrier, bool phase_parity, uint32_t max_sleep_nanosec) {
    return _LWDA_AWBARRIER_SM_TARGET::__awbarrier_try_wait_parity(barrier, phase_parity, max_sleep_nanosec);
}

_LWDA_AWBARRIER_END_INTERNAL_NAMESPACE

#endif /* !_LWDA_AWBARRIER_SM_TARGET */

#endif /* !_LWDA_AWBARRIER_HELPERS_H_ */
