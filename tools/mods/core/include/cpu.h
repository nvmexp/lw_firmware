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

// Interface to an x86 CPU.

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_CPU_H
#define INCLUDED_CPU_H

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif
#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

namespace Cpu
{
    enum MemoryType
    {
        Normal         = 0x00000000,
        Uncacheable    = 0x00000001,
        WriteCombining = 0x00000002,
        WriteThrough   = 0x00000004,
        WriteProtected = 0x00000008,
        WriteBack      = 0x00000010,
    };

    enum Feature
    {
        CRC32C_INSTRUCTION
    };

    //! Read CPU caps, etc. so we don't have to check them later
    RC Initialize();

    //! Pause for a short period of time, trying to put the CPU into a low-power
    // state or a state that consumes less resources on an SMT system if possible.
    void Pause();

    //! Determine whether the CPUID instruction is supported
    bool IsCPUIDSupported();

    bool IsFeatureSupported(Feature feature);

    //! Assembly instruction(s) for breakpoints
    void BreakPoint();

    //! Get the results of an x86 CPUID instruction
    //! Can pass in a null pointer for a register if not interested in its value.
    void CPUID
    (
        UINT32   Op,
        UINT32   SubOp,
        UINT32 * pEAX,
        UINT32 * pEBX,
        UINT32 * pECX,
        UINT32 * pEDX
    );

    //! Get CPU name
    const char* Name();

    //! Get CPU architecture
    const char* Architecture();

    //! A multi-thread load barrier, which works on both compiler and hardware level.
    //!
    //! This should be performed before accessing shared data without enforced
    //! memory ordering (e.g. via other atomic ops).
    void ThreadFenceAcquire();

    //! A multi-thread store barrier, which works on both compiler and hardware level.
    //!
    //! This should be performed after writing shared data without enforced
    //! memory ordering (e.g. via other atomic ops).
    void ThreadFenceRelease();

    //! Perform an atomic load.
    bool  AtomicRead(volatile bool*  pValue);
    INT32 AtomicRead(volatile INT32* pValue);
    UINT32 AtomicRead(volatile UINT32* pValue);
    INT64 AtomicRead(volatile INT64* pValue);
    UINT64 AtomicRead(volatile UINT64* pValue);

    //! Perform an atomic store.
    void AtomicWrite(volatile bool*  pValue, bool  data);
    void AtomicWrite(volatile INT32* pValue, INT32 data);
    void AtomicWrite(volatile UINT32* pValue, UINT32 data);
    void AtomicWrite(volatile INT64* pValue, INT64 data);
    void AtomicWrite(volatile UINT64* pValue, UINT64 data);

    //! Perform an atomic exchange of a value with a memory location
    UINT32 AtomicXchg32(volatile UINT32 *Addr, UINT32 Data);

    //! Atomically update the value in a memory location iff the
    //! location lwrrently contains OldVal.  Return true on success.
    bool AtomicCmpXchg32(volatile UINT32 *Addr, UINT32 OldVal, UINT32 NewVal);
    bool AtomicCmpXchg64(volatile UINT64 *Addr, UINT64 OldVal, UINT64 NewVal);

    //! Perform an atomic add.
    //! @return Returns previous value.
    INT32 AtomicAdd(volatile INT32* pValue, INT32 data);

    //! Perform an atomic add.
    //! @return Returns previous value.
    UINT32 AtomicAdd(volatile UINT32* pValue, UINT32 data);

    //! Perform an atomic add for INT64.
    //! @return Returns previous value.
    INT64 AtomicAdd(volatile INT64* pValue, INT64 data);

    //! Perform an atomic add for UINT64.
    //! @return Returns previous value.
    UINT64 AtomicAdd(volatile UINT64* pValue, UINT64 data);

    //! Set the floating-point control word and return its old value.
    UINT32 SetFpControlWord(UINT32 NewFpcw);

    //! Get the current floating-point control word.
    UINT32 GetFpControlWord();

    //! Set the SSE floating-point control word and return its old value.
    UINT32 SetMXCSRControlWord(UINT32 NewFpcw);

    //! Get the current SSE floating-point control word.
    UINT32 GetMXCSRControlWord();

    RC RdIdt(UINT32 *pBase, UINT32 *pLimit);

    //! Ilwalidate the CPU's cache without flushing it.  (Very dangerous!)
    RC IlwalidateCache();

    //! Flush the CPU's cache.
    RC FlushCache();

    //! Flush the CPU's cache from 'StartAddress' to 'EndAddress'.
    RC FlushCpuCacheRange(void * StartAddress, void * EndAddress, UINT32 Flags);

    //! Flush the CPU's write combine buffer.
    RC FlushWriteCombineBuffer();

    //! Efficiently copy data from src to dst
    //! Uses SSE 16-byte r/w instructions when available.
    void MemCopy(void *dst, const void *src, size_t n);

    //! Return true if we have an optimized MemCopy routine that
    //! uses wider-than-4-bytes load/store instructions.
    //! I.e. true if SSE intrinsics or inline-asm are used.
    bool HasWideMemCopy ();

    //! Efficiently fill dst buffer with data
    void MemSet(void *dst, UINT08 data, size_t n);
}

#endif // ! INCLUDED_CPU_H
