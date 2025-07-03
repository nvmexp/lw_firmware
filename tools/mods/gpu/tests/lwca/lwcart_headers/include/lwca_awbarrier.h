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

#ifndef _LWDA_AWBARRIER_H_
# define _LWDA_AWBARRIER_H_

# include "lwda_awbarrier_primitives.h"

# if !defined(_LWDA_AWBARRIER_SM_TARGET)
#  error This file requires compute capability 7.0 or greater.
# endif

# if !defined(_LWDA_AWBARRIER_CPLUSPLUS_11_OR_LATER)
#  error This file requires compiler support for the ISO C++ 2011 standard. This support must be enabled with the \
             -std=c++11 compiler option.
# endif

_LWDA_AWBARRIER_BEGIN_NAMESPACE

class awbarrier {
public:
    class arrival_token {
    public:
        arrival_token() = default;
        ~arrival_token() = default;
        _LWDA_AWBARRIER_QUALIFIER uint32_t pending_count() const;
    private:
        _LWDA_AWBARRIER_QUALIFIER arrival_token(uint64_t token);
        uint64_t token;
        friend awbarrier;
    };
    awbarrier() = default;
    awbarrier(const awbarrier&) = delete;
    awbarrier& operator=(const awbarrier&) = delete;
    ~awbarrier() = default;

    _LWDA_AWBARRIER_QUALIFIER arrival_token arrive();
    _LWDA_AWBARRIER_QUALIFIER arrival_token arrive_and_drop();
    _LWDA_AWBARRIER_QUALIFIER bool timed_wait(arrival_token token, uint32_t hint_cycles);
    _LWDA_AWBARRIER_QUALIFIER void wait(arrival_token token);
    _LWDA_AWBARRIER_QUALIFIER void arrive_and_wait();
    _LWDA_AWBARRIER_STATIC_QUALIFIER __host__ constexpr uint32_t max();
private:
    uint64_t barrier;
    friend _LWDA_AWBARRIER_QUALIFIER void init(awbarrier* barrier, uint32_t expected_count);
    friend _LWDA_AWBARRIER_QUALIFIER void ilwal(awbarrier* barrier);
    friend class pipeline;
};

_LWDA_AWBARRIER_QUALIFIER
uint32_t awbarrier::arrival_token::pending_count() const
{
    const uint32_t pending_count = _LWDA_AWBARRIER_INTERNAL_NAMESPACE::awbarrier_token_pending_count(this->token);
#if (__LWDA_ARCH__ >= 900)
    return pending_count;
#else
    return (pending_count >> 15);
#endif
}

_LWDA_AWBARRIER_QUALIFIER
awbarrier::arrival_token::arrival_token(uint64_t token)
    : token(token)
{
}

_LWDA_AWBARRIER_QUALIFIER
void init(awbarrier* barrier, uint32_t expected_count)
{
    _LWDA_AWBARRIER_ASSERT(__isShared(barrier));
    _LWDA_AWBARRIER_ASSERT(expected_count > 0 && expected_count <= _LWDA_AWBARRIER_MAX_COUNT);

#if (__LWDA_ARCH__ >= 900)
    const uint32_t init_count = expected_count;
#else
    const uint32_t init_count = (expected_count << 15) + expected_count;
#endif

    _LWDA_AWBARRIER_INTERNAL_NAMESPACE::awbarrier_init(&barrier->barrier, init_count);
}

_LWDA_AWBARRIER_QUALIFIER
void ilwal(awbarrier* barrier)
{
    _LWDA_AWBARRIER_ASSERT(__isShared(barrier));

    _LWDA_AWBARRIER_INTERNAL_NAMESPACE::awbarrier_ilwal(&barrier->barrier);
}

_LWDA_AWBARRIER_QUALIFIER
awbarrier::arrival_token awbarrier::arrive()
{
    _LWDA_AWBARRIER_ASSERT(__isShared(&this->barrier));

#if (__LWDA_ARCH__ >= 900)
    const uint32_t arrive_count = 1;
#else
    const uint32_t arrive_count = 1 << 15;
#endif
    const uint64_t token = _LWDA_AWBARRIER_INTERNAL_NAMESPACE::awbarrier_arrive_drop_no_complete<false>(&this->barrier, arrive_count);

    (void)_LWDA_AWBARRIER_INTERNAL_NAMESPACE::awbarrier_arrive_drop<false>(&this->barrier);

    return arrival_token(token);
}

_LWDA_AWBARRIER_QUALIFIER
awbarrier::arrival_token awbarrier::arrive_and_drop()
{
    _LWDA_AWBARRIER_ASSERT(__isShared(&this->barrier));

#if (__LWDA_ARCH__ >= 900)
    const uint32_t arrive_count = 1;
#else
    const uint32_t arrive_count = 1 << 15;
#endif
    const uint64_t token = _LWDA_AWBARRIER_INTERNAL_NAMESPACE::awbarrier_arrive_drop_no_complete<true>(&this->barrier, arrive_count);

    (void)_LWDA_AWBARRIER_INTERNAL_NAMESPACE::awbarrier_arrive_drop<true>(&this->barrier);

    return arrival_token(token);
}

_LWDA_AWBARRIER_QUALIFIER
bool awbarrier::timed_wait(arrival_token token, uint32_t hint_cycles)
{
    constexpr uint64_t max_busy_wait_cycles = 1024;
    constexpr uint32_t max_sleep_ns = 1 << 20;

    _LWDA_AWBARRIER_ASSERT(__isShared(&this->barrier));

    if (_LWDA_AWBARRIER_INTERNAL_NAMESPACE::awbarrier_test_wait(&this->barrier, token.token)) {
        return true;
    }

    uint64_t start_cycles = clock64();
    uint64_t elapsed_cycles = 0;
    uint32_t sleep_ns = 32;
    while (elapsed_cycles < hint_cycles) {
        if (_LWDA_AWBARRIER_INTERNAL_NAMESPACE::awbarrier_test_wait(&this->barrier, token.token)) {
            return true;
        }

        if (elapsed_cycles > max_busy_wait_cycles) {
            __nanosleep(sleep_ns);
            if (sleep_ns < max_sleep_ns) {
                sleep_ns *= 2;
            }
        }

        elapsed_cycles = clock64() - start_cycles;
    }

    return false;
}

_LWDA_AWBARRIER_QUALIFIER
void awbarrier::wait(arrival_token token)
{
    _LWDA_AWBARRIER_ASSERT(__isShared(&this->barrier));

    while (!timed_wait(token, ~0u));
}

_LWDA_AWBARRIER_QUALIFIER
void awbarrier::arrive_and_wait()
{
    _LWDA_AWBARRIER_ASSERT(__isShared(&this->barrier));

    this->wait(this->arrive());
}

_LWDA_AWBARRIER_QUALIFIER __host__
constexpr uint32_t awbarrier::max()
{
    return _LWDA_AWBARRIER_MAX_COUNT;
}

_LWDA_AWBARRIER_END_NAMESPACE

#endif /* !_LWDA_AWBARRIER_H_ */
