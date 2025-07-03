/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2016,2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Interface to an AMD64 CPU.

#include "core/include/cpu.h"
#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/xp.h"
#include "core/include/utility.h"
#include <float.h>
#include <windows.h>
#include <intrin.h>

static char   s_Name[49];
static UINT32 s_FeatureCX;
static UINT32 s_FeatureDX;

RC Cpu::Initialize()
{
    UINT32 FamilyModelStepping;
    CPUID(1, 0, &FamilyModelStepping, NULL, &s_FeatureCX, &s_FeatureDX);

    UINT32 HighestExtLevel = 0;
    CPUID(0x80000000, 0, &HighestExtLevel, 0, 0, 0);
    if (HighestExtLevel >= 0x80000004)
    {
        CPUID(0x80000002, 0, (UINT32*)&s_Name[0], (UINT32*)&s_Name[4],
                (UINT32*)&s_Name[8], (UINT32*)&s_Name[12]);
        CPUID(0x80000003, 0, (UINT32*)&s_Name[16], (UINT32*)&s_Name[20],
                (UINT32*)&s_Name[24], (UINT32*)&s_Name[28]);
        CPUID(0x80000004, 0, (UINT32*)&s_Name[32], (UINT32*)&s_Name[36],
                (UINT32*)&s_Name[40], (UINT32*)&s_Name[44]);

        // Remove duplicate, leading and trailing spaces
        int  idest     = 0;
        bool hadSpace = false;

        for (unsigned isrc = 0; isrc < sizeof(s_Name) - 1; isrc++)
        {
            const char c = s_Name[isrc];

            // Note down blanks, i.e. space, TAB, LF, NUL, etc.
            if (static_cast<unsigned char>(c) <= 0x20)
            {
                hadSpace = true;
            }
            else
            {
                // Emit space, unless it's the first non-blank character
                if (hadSpace && idest)
                {
                    s_Name[idest++] = ' ';
                }

                // Emit printable, non-blank character
                s_Name[idest++] = c;
                hadSpace = false;
            }
        }

        // NUL-terminate, idest cannot exceed s_Name size
        s_Name[idest] = 0;
    }

    return OK;
}

void Cpu::Pause()
{
    _mm_pause();
}

#ifndef LW_WINDOWS
void Cpu::BreakPoint()
{
    __asm__("int $3");
}
#endif

bool Cpu::IsCPUIDSupported()
{
    return true;
}

bool Cpu::IsFeatureSupported(Feature feature)
{
    switch (feature)
    {
        case CRC32C_INSTRUCTION:
            return (s_FeatureCX & (1 << 20)) != 0; // Since SSE4.2
        default:
            return false;
    }
}

void Cpu::CPUID
(
    UINT32   Op,
    UINT32   SubOp,
    UINT32 * pEAX,
    UINT32 * pEBX,
    UINT32 * pECX,
    UINT32 * pEDX
)
{
    int out[4];

    __cpuidex(out, Op, SubOp);

    if (pEAX) *pEAX = static_cast<UINT32>(out[0]);
    if (pEBX) *pEBX = static_cast<UINT32>(out[1]);
    if (pECX) *pECX = static_cast<UINT32>(out[2]);
    if (pEDX) *pEDX = static_cast<UINT32>(out[3]);
}

const char* Cpu::Architecture()
{
    return "x86_64";
}

const char* Cpu::Name()
{
    return s_Name;
}

//------------------------------------------------------------------------------
UINT32 Cpu::SetFpControlWord(UINT32 NewFpcw)
{
    UINT32 oldControlFp = GetFpControlWord();
    _controlfp(NewFpcw, 0xFFFFFFFF);
    return oldControlFp;
}

UINT32 Cpu::GetFpControlWord()
{
    return _controlfp(0, 0);
}

UINT32 Cpu::SetMXCSRControlWord(UINT32 NewMxcsr)
{
    UINT32 OldMxcsr = _mm_getcsr();
    _mm_setcsr(NewMxcsr);
    return OldMxcsr;
}

UINT32 Cpu::GetMXCSRControlWord()
{
    return _mm_getcsr();
}

RC Cpu::RdIdt(UINT32 *pBase, UINT32 *pLimit)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Cpu::IlwalidateCache()
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Cpu::FlushCache()
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Cpu::FlushCpuCacheRange(void * StartAddress, void * EndAddress, /*IGNORED*/ UINT32 Flags)
{
    if (StartAddress > EndAddress)
    {
        return RC::BAD_PARAMETER;
    }

#if 0
    // XXX assume 64 bytes
    UINT32 CacheBlockSize = 64;
#else
    // Figure out how large the cache lines are
    // EBX: Bits 15-8: CLFLUSH line size. (Value * 8 = cache line size in bytes)
    UINT32 tmpEAX, tmpEBX, tmpECX, tmpEDX;
    CPUID(1, 0, &tmpEAX, &tmpEBX, &tmpECX, &tmpEDX);
    UINT32 CacheBlockSize = ((tmpEBX & 0x0000FF00) >> 8) * 8;
#endif

    // Flush the cpu cache blocks
    for
    (
        char * Address = reinterpret_cast<char*>(StartAddress);
        Address <= reinterpret_cast<char*>(EndAddress);
        Address += CacheBlockSize
    )
    {
        _mm_clflush(Address);
    }

    // Make sure we got the end address as we're not guaranteed to start
    // flushing at an addr aligned with CacheBlockSize
    _mm_clflush(EndAddress);

    return OK;
}

RC Cpu::FlushWriteCombineBuffer()
{
    _mm_sfence();
    return OK;
}

void Cpu::MemCopy(void *dst, const void *src, size_t n)
{
    // @@@
    // Unless the runtime library memcpy is proven to use SSE instructions,
    // this is inadequate for full HW test coverage.
    // Need inline asm here.
    memcpy(dst, src, n);
}

bool Cpu::HasWideMemCopy ()
{
    return false;
}

void Cpu::MemSet(void *dst, UINT08 data, size_t n)
{
    memset(dst, data, n);
}
