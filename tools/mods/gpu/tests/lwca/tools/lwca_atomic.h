/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "../tools/tools.h"

// liblw++ library fully supported on SM_VER >= 70
// partial support for SM_VER >= 60
#if SM_VER >= 60
#include <lwca/atomic>
template<typename T, lwca::thread_scope S = lwca::thread_scope_device>
__device__ __forceinline__ lwca::atomic_ref<T, S> GetAtomicRef(device_ptr ptr, size_t offset = 0)
{
    return lwca::atomic_ref<T, S>(GetPtr<T*>(ptr)[offset]);
}
#endif

__device__ __forceinline__ UINT64 IncrementErrCount(device_ptr errorCountPtr)
{
#if SM_VER >= 60
    auto errorCountAtom = GetAtomicRef<UINT64, lwca::thread_scope_device>(errorCountPtr);
    return errorCountAtom.fetch_add(1, lwca::memory_order_relaxed);
#else
    UINT64* pErrorCount = GetPtr<UINT64*>(errorCountPtr);
    return atomicAdd(pErrorCount, 1);
#endif
}
