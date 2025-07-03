/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Interface to an x86_64 CPU.

#include "core/include/cpu.h"
#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/xp.h"
#include "core/include/utility.h"
#include <xmmintrin.h>

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

void Cpu::BreakPoint()
{
    __asm__("int $3");
}

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
    UINT32 Temp;

    if (!pEAX) pEAX = &Temp;
    if (!pEBX) pEBX = &Temp;
    if (!pECX) pECX = &Temp;
    if (!pEDX) pEDX = &Temp;

    __asm__ volatile
    (
        // Run cpuid instruction.
         "cpuid\n"
        // output
        : "=a" (*pEAX), "=b" (*pEBX), "=c" (*pECX), "=d" (*pEDX)
        // input
        : "a" (Op), "c" (SubOp)
    );
}

const char* Cpu::Architecture()
{
    return "x86_64";
}

const char* Cpu::Name()
{
    return s_Name;
}

#define SAVE_FP_CONTROL_WORD(_fpCW_) { \
    __asm__ __volatile__("fstcw %0" : "=m" (_fpCW_) : : "memory");      \
}

#define SET_FP_CONTROL_WORD(_fpCW_) {                                   \
    volatile unsigned int lVal = _fpCW_;                                \
    __asm__("fldcw %0" : : "m" (lVal));                                 \
}

#define SAVE_MXCSR_CONTROL_WORD(_mxcsr_) { \
    __asm__ __volatile__("stmxcsr %0" : "=m" (_mxcsr_) : : "memory");   \
}

#define SET_MXCSR_CONTROL_WORD(_mxcsr_) {                               \
    volatile unsigned int lVal = _mxcsr_;                               \
    __asm__("ldmxcsr %0" : : "m" (lVal));                               \
}

UINT32 Cpu::SetFpControlWord(UINT32 NewFpcw)
{
    UINT32 OldFpcw;

    SAVE_FP_CONTROL_WORD(OldFpcw);
    SET_FP_CONTROL_WORD(NewFpcw);

    return OldFpcw;
}

UINT32 Cpu::GetFpControlWord()
{
    UINT32 Fpcw;

    SAVE_FP_CONTROL_WORD(Fpcw);

    return Fpcw;
}

UINT32 Cpu::SetMXCSRControlWord(UINT32 NewMxcsr)
{
    UINT32 OldMxcsr;

    SAVE_MXCSR_CONTROL_WORD(OldMxcsr);
    SET_MXCSR_CONTROL_WORD(NewMxcsr);

    return OldMxcsr;
}

UINT32 Cpu::GetMXCSRControlWord()
{
    UINT32 Mxcsr;

    SAVE_MXCSR_CONTROL_WORD(Mxcsr);

    return Mxcsr;
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

RC Cpu::FlushCpuCacheRange(void * StartAddress, void * EndAddress, /*Ignored*/ UINT32 Flags)
{
    if (StartAddress > EndAddress)
    {
        return RC::BAD_PARAMETER;
    }

    // Figure out how large the cache lines are
    UINT32 tmpEAX, tmpEBX, tmpECX, tmpEDX;
    CPUID(1, 0, &tmpEAX, &tmpEBX, &tmpECX, &tmpEDX);

    // EBX: Bits 15-8: CLFLUSH line size. (Value * 8 = cache line size in bytes)
    UINT32 CacheBlockSize = ((tmpEBX & 0x0000FF00) >> 8) * 8;

    // Flush the cpu cache blocks
    for
    (
        char * Address = reinterpret_cast<char*>(StartAddress);
        Address <= reinterpret_cast<char*>(EndAddress);
        Address += CacheBlockSize
    )
    {
        __asm__
        (
            "clflush %0"
            /* output */
            :
            /* input */
            : "m"(Address)
        );
    }

    // Make sure we got the end address as we're not guaranteed to start
    // flushing at an addr aligned with CacheBlockSize
    __asm__
    (
        "clflush %0"
        /* output */
        :
        /* input */
        : "m"(EndAddress)
    );

    return OK;
}

RC Cpu::FlushWriteCombineBuffer()
{
    __asm__ volatile ("sfence");
    return OK;
}

void Cpu::MemSet(void *dst, UINT08 data, size_t n)
{
    memset(dst, data, n);
}

void Cpu::MemCopy(void *dst, const void *src, size_t n)
{
    // Easier to work on things in terms of integers when dealing with alignment.
    uintptr_t d = (uintptr_t)dst;
    uintptr_t s = (uintptr_t)src;

    // Align dst to 4 bytes
    if (d & 3)
    {
        size_t num = 4 - (d & 3); // how many bytes we need to get aligned
        if (n >= num) // do we have at least that many in total to copy?
        {
            // Copy 1 byte at a time until we are aligned
            while (d & 3)
            {
                *(UINT08 *)d = *(const UINT08 *)s;
                d++;
                s++;
                n--;
            }
        }
    }

    // Align dst to 16 bytes
    if (d & 15)
    {
        size_t num = 16 - (d & 15); // how many bytes we need to get aligned
        if (n >= num) // do we have at least that many in total to copy?
        {
            if (s & 3) // is src 4 byte aligned?
            {
                // Copy 1 byte at a time until dest pointer is aligned
                while (d & 15)
                {
                    *(UINT08 *)d = *(const UINT08 *)s;
                    d += 1;
                    s += 1;
                    n -= 1;
                }
            }
            else
            {
                // Copy 4 bytes at a time until dest pointer is aligned
                while (d & 15)
                {
                    *(UINT32 *)d = *(const UINT32 *)s;
                    d += 4;
                    s += 4;
                    n -= 4;
                }
            }
        }
    }

    if (s & 15)
    {
        // Misaligned source
        while (n >= 16)
        {
            _mm_store_ps((float*)d, _mm_loadu_ps((float*)s));
            d += 16;
            s += 16;
            n -= 16;
        }
    }
    else
    {
        // Aligned source
        while (n >= 16)
        {
            _mm_store_ps((float*)d, _mm_load_ps((float*)s));
            d += 16;
            s += 16;
            n -= 16;
        }
    }

    // 4 byte aligned source & destination & >= 4 bytes to copy
    if (!(d & 3) && !(s & 3) && (n >= 4))
    {
        while (n >= 4)
        {
            *(UINT32 *)d = *(const UINT32 *)s;
            d += 4;
            s += 4;
            n -= 4;
        }
    }

    while (n)
    {
        *(UINT08 *)d = *(const UINT08 *)s;
        d++;
        s++;
        n--;
    }
}

bool Cpu::HasWideMemCopy ()
{
    return true;
}
