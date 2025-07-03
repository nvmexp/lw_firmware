/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Cpu object, properties, and methods.

#include "core/include/cpu.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "core/include/xp.h"
#include <atomic>

namespace
{
    template<typename T>
    std::atomic<T>& CppAtomic(volatile T* value)
    {
        static_assert(sizeof(T) == sizeof(std::atomic<T>), "std::atomic<T> must be of the same size as T");
        return *reinterpret_cast<std::atomic<T>*>(const_cast<T*>(value));
    }
}

void Cpu::ThreadFenceAcquire()
{
    atomic_thread_fence(memory_order_acquire);
}

void Cpu::ThreadFenceRelease()
{
    atomic_thread_fence(memory_order_release);
}

// Note: The C++11 atomic operations below use sequentially consistent
// memory ordering by default, using a weaker ordering could break
// the assumptions in the algorightms which rely on these functions.

bool Cpu::AtomicRead(volatile bool* addr)
{
    return CppAtomic(addr).load();
}

INT32 Cpu::AtomicRead(volatile INT32* addr)
{
    return CppAtomic(addr).load();
}

UINT32 Cpu::AtomicRead(volatile UINT32* addr)
{
    return CppAtomic(addr).load();
}

INT64 Cpu::AtomicRead(volatile INT64* addr)
{
    return CppAtomic(addr).load();
}

UINT64 Cpu::AtomicRead(volatile UINT64* addr)
{
    return CppAtomic(addr).load();
}

void Cpu::AtomicWrite(volatile bool* addr, bool data)
{
    CppAtomic(addr).store(data);
}

void Cpu::AtomicWrite(volatile INT32* addr, INT32 data)
{
    CppAtomic(addr).store(data);
}

void Cpu::AtomicWrite(volatile UINT32* addr, UINT32 data)
{
    CppAtomic(addr).store(data);
}

void Cpu::AtomicWrite(volatile INT64* addr, INT64 data)
{
    CppAtomic(addr).store(data);
}

void Cpu::AtomicWrite(volatile UINT64* addr, UINT64 data)
{
    CppAtomic(addr).store(data);
}

UINT32 Cpu::AtomicXchg32(volatile UINT32* addr, UINT32 data)
{
    return CppAtomic(addr).exchange(data);
}

bool Cpu::AtomicCmpXchg32
(
    volatile UINT32* addr,
    UINT32           oldValue,
    UINT32           newValue
)
{
    return CppAtomic(addr).compare_exchange_strong(oldValue, newValue);
}

bool Cpu::AtomicCmpXchg64
(
    volatile UINT64* addr,
    UINT64           oldValue,
    UINT64           newValue
)
{
    return CppAtomic(addr).compare_exchange_strong(oldValue, newValue);
}

INT32 Cpu::AtomicAdd(volatile INT32* addr, INT32 data)
{
    return CppAtomic(addr).fetch_add(data);
}

UINT32 Cpu::AtomicAdd(volatile UINT32* addr, UINT32 data)
{
    return CppAtomic(addr).fetch_add(data);
}

INT64 Cpu::AtomicAdd(volatile INT64* addr, INT64 data)
{
    return CppAtomic(addr).fetch_add(data);
}

UINT64 Cpu::AtomicAdd(volatile UINT64* addr, UINT64 data)
{
    return CppAtomic(addr).fetch_add(data);
}

#ifndef VULKAN_STANDALONE

JS_CLASS(Cpu);

static SObject Cpu_Object
(
   "Cpu",
   CpuClass,
   0,
   0,
   "Cpu interface."
);

//
// Properties
//

static SProperty Cpu_Normal
(
   Cpu_Object,
   "Normal",
   0,
   Cpu::Normal,
   0,
   0,
   JSPROP_READONLY,
   "Normal memory range."
);

static SProperty Cpu_Uncacheable
(
   Cpu_Object,
   "Uncacheable",
   0,
   Cpu::Uncacheable,
   0,
   0,
   JSPROP_READONLY,
   "Uncacheable memory range."
);

static SProperty Cpu_WriteCombining
(
   Cpu_Object,
   "WriteCombining",
   0,
   Cpu::WriteCombining,
   0,
   0,
   JSPROP_READONLY,
   "WriteCombining memory range."
);

static SProperty Cpu_WriteThrough
(
   Cpu_Object,
   "WriteThrough",
   0,
   Cpu::WriteThrough,
   0,
   0,
   JSPROP_READONLY,
   "WriteThrough memory range."
);

static SProperty Cpu_WriteProtected
(
   Cpu_Object,
   "WriteProtected",
   0,
   Cpu::WriteProtected,
   0,
   0,
   JSPROP_READONLY,
   "WriteProtected memory range."
);

static SProperty Cpu_WriteBack
(
   Cpu_Object,
   "WriteBack",
   0,
   Cpu::WriteBack,
   0,
   0,
   JSPROP_READONLY,
   "WriteBack memory range."
);

P_(Cpu_Get_Arch);
static SProperty Cpu_Arch
(
   Cpu_Object,
   "Arch",
   0,
   0,
   Cpu_Get_Arch,
   0,
   0,
   "CPU architecture"
);

P_(Cpu_Get_Name);
static SProperty Cpu_Name
(
   Cpu_Object,
   "Name",
   0,
   0,
   Cpu_Get_Name,
   0,
   0,
   "CPU name"
);

P_(Cpu_Get_Cores);
static SProperty Cpu_Cores
(
   Cpu_Object,
   "Cores",
   0,
   0,
   Cpu_Get_Cores,
   0,
   0,
   "Number of CPU cores"
);

//
// Methods and Tests
//

C_(Cpu_FlushCache);
static STest Cpu_FlushCache
(
   Cpu_Object,
   "FlushCache",
   C_Cpu_FlushCache,
   0,
   "Write back and ilwalidate the CPU cache."
);

//
// Implementation
//

P_(Cpu_Get_Arch)
{
    RC rc;
    JavaScriptPtr pJs;
    C_CHECK_RC_THROW(pJs->ToJsval(Cpu::Architecture(), pValue),
                     "Failed to get CPU architecture");
    return JS_TRUE;
}

P_(Cpu_Get_Name)
{
    RC rc;
    JavaScriptPtr pJs;
    C_CHECK_RC_THROW(pJs->ToJsval(Cpu::Name(), pValue), "Failed to get CPU foundry");
    return JS_TRUE;
}

P_(Cpu_Get_Cores)
{
    RC rc;
    JavaScriptPtr pJs;
    C_CHECK_RC_THROW(pJs->ToJsval(Xp::GetNumCpuCores(), pValue),
                     "Failed to get number of CPU cores");
    return JS_TRUE;
}

// STest
C_(Cpu_FlushCache)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // Check this is a void method.
   if (NumArguments != 0)
   {
      JS_ReportError(pContext, "Usage: Cpu.FlushCache()");
      return JS_FALSE;
   }

   RETURN_RC(Cpu::FlushCache());
}

#endif
