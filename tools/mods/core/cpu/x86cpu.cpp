/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2012,2014-2016,2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Interface to an x86 CPU.

#include "core/include/cpu.h"
#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/xp.h"
#include "core/include/utility.h"

#ifdef LW_WINDOWS
#include <xmmintrin.h>
#endif

#define CPU_STD_TSC   (1 << 4)
#define CPU_STD_CMOV  (1 << 15)
#define CPU_STD_CLFSH (1 << 19)
#define CPU_STD_MMX   (1 << 23)
#define CPU_STD_FXSR  (1 << 24)
#define CPU_STD_SSE   (1 << 25)
#define CPU_STD_SSE2  (1 << 26)

static bool   s_CPUIDSupported;
static char   s_Name[49] = {0};
static UINT32 s_StandardFeatures;
static UINT32 s_CXFeatures;
static UINT32 s_MemoryFeatures[4];

const char * Foundries[] =
{
    "Generic",
    "GenuineIntel",
    "AuthenticAMD",
    "CentaurHauls",
    "CyrixInstead"
};

enum Foundry
{
    Generic,
    Intel,
    AMD,
    WinChip,
    Cyrix,
};

RC Cpu::Initialize()
{
    UINT32 t0, t1;

    // Check if cpuid instruction is supported.
    // CPUs which do not support cpuid instruction hardwire bit 21 of
    // eflag to 0.
#if defined(LW_WINDOWS)
    __asm
    {
        pushfd
        pop      eax
        mov      t0, eax
        xor      eax, 0x00200000
        push     eax
        popfd
        pushfd
        pop      eax
        mov      t1, eax
    };
#else
    __asm__ volatile
    (
        "   pushfl                    \n"
        "   popl     %0               \n"
        "   movl     %0, %1           \n"
        "   xorl     $0x00200000, %0  \n"
        "   pushl    %0               \n"
        "   popfl                     \n"
        "   pushfl                    \n"
        "   popl     %0               \n"
        // output
        : "=r" (t0), "=r" (t1)
        // input
        :
        // modified
        : "cc" // Modified condition code register, i.e. eflags.
    );
#endif
    s_CPUIDSupported = (t0 != t1);

    if (s_CPUIDSupported)
    {
        UINT32 FamilyModelStepping;

        CPUID(1, 0, &FamilyModelStepping, NULL, &s_CXFeatures, &s_StandardFeatures);

        CPUID(2, 0, &s_MemoryFeatures[0], &s_MemoryFeatures[1],
              &s_MemoryFeatures[2], &s_MemoryFeatures[3]);

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
    }

    return OK;
}

void Cpu::Pause()
{
    // Use PAUSE instruction for reduced power consumption inside spin loops.  A
    // better implementation would be to do multiple PAUSE instructions, or to
    // even loop PAUSE until a given amount of time has passed.
#ifdef LW_WINDOWS
    __asm pause
#else
    __asm__(".byte 0xf3\n\t"
            ".byte 0x90\n\t");
#endif
}

#ifndef LW_WINDOWS
void Cpu::BreakPoint()
{
    __asm__("int $3");
}
#endif

bool Cpu::IsCPUIDSupported()
{
    return s_CPUIDSupported;
}

bool Cpu::IsFeatureSupported(Feature feature)
{
    switch (feature)
    {
        case CRC32C_INSTRUCTION:
            return (s_CXFeatures & (1 << 20)) != 0; // Since SSE4.2
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

#if defined(LW_WINDOWS)
    __asm
    {
        mov eax, Op
        mov ecx, SubOp
        cpuid
        mov edi, pEAX
        mov [edi], eax
        mov edi, pEBX
        mov [edi], ebx
        mov edi, pECX
        mov [edi], ecx
        mov edi, pEDX
        mov [edi], edx
    };
#else
    __asm__ volatile
    (
        // Run cpuid instruction.  Note: EBX cannot change if gcc
        // compiles with -fPIC flag (default on mac).
        // Use esi to store the contents of ebx from the call to cpuid
        // since gcc 4 picks ebx if given the choice and this results in the
        // data being lost
        "push %%ebx\n"
        "cpuid\n"
        "mov %%ebx, %1\n"
        "pop %%ebx"
        // output
        : "=a" (*pEAX), "=&S" (*pEBX), "=c" (*pECX), "=d" (*pEDX)
        // input
        : "a" (Op), "c" (SubOp)
    );
#endif
}

const char* Cpu::Architecture()
{
    return "x86";
}

const char* Cpu::Name()
{
    return s_Name;
}

//------------------------------------------------------------------------------
#if !defined(LW_WINDOWS)
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
#endif

UINT32 Cpu::SetFpControlWord(UINT32 NewFpcw)
{
    UINT32 OldFpcw;
#if defined(LW_WINDOWS)
    __asm fstcw OldFpcw
    __asm fldcw NewFpcw
#else
    SAVE_FP_CONTROL_WORD(OldFpcw);
    SET_FP_CONTROL_WORD(NewFpcw);
#endif
    return OldFpcw;
}

UINT32 Cpu::GetFpControlWord()
{
    UINT32 Fpcw;
#if defined(LW_WINDOWS)
    __asm fstcw Fpcw
#else
    SAVE_FP_CONTROL_WORD(Fpcw);
#endif
    return Fpcw;
}

UINT32 Cpu::SetMXCSRControlWord(UINT32 NewMxcsr)
{
    UINT32 OldMxcsr;
#if defined(LW_WINDOWS)
    __asm stmxcsr OldMxcsr
    __asm ldmxcsr NewMxcsr
#else
    SAVE_MXCSR_CONTROL_WORD(OldMxcsr);
    SET_MXCSR_CONTROL_WORD(NewMxcsr);
#endif
    return OldMxcsr;
}

UINT32 Cpu::GetMXCSRControlWord()
{
    UINT32 Mxcsr;
#if defined(LW_WINDOWS)
    __asm stmxcsr Mxcsr
#else
    SAVE_MXCSR_CONTROL_WORD(Mxcsr);
#endif
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

RC Cpu::FlushCpuCacheRange(void * StartAddress, void * EndAddress, /*IGNORED*/ UINT32 Flags)
{
    // If the CLFLUSH instruction isn't supported, just flush the whole thing
    if (!(s_StandardFeatures & CPU_STD_CLFSH))
    {
        return FlushCache();
    }

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
#ifdef LW_WINDOWS
        __asm
        {
            mov eax, Address
            clflush [eax]
        };
#else
        __asm__
        (
            "clflush %0"
            /* output */
            :
            /* input */
            : "m"(Address)
        );
#endif
    }

    // Make sure we got the end address as we're not guaranteed to start
    // flushing at an addr aligned with CacheBlockSize
#ifdef LW_WINDOWS
    __asm
    {
        mov eax, EndAddress
        clflush [eax]
    };
#else
    __asm__
    (
        "clflush %0"
        /* output */
        :
        /* input */
        : "m"(EndAddress)
    );
#endif

    return OK;
}

RC Cpu::FlushWriteCombineBuffer()
{
    // Use sfence instruction, if it's available
    if (s_StandardFeatures & CPU_STD_SSE)
    {
#ifdef LW_WINDOWS
        __asm sfence;
#else
        __asm__ volatile ("sfence");
#endif
        return OK;
    }

    // XXX There are other ways we could do this
    return RC::UNSUPPORTED_FUNCTION;
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

    if (s_StandardFeatures & CPU_STD_SSE)
    {
        // SSE supported, use it
#ifdef LW_WINDOWS
        __m128 t;
#endif

        // Align dst to 16 bytes
        if (d & 15)
        {
            size_t num = 16 - (d & 15); // how many bytes we need to get aligned
            if (n >= num) // do we have at least that many in total to copy?
            {
                // Copy 4 bytes at a time until we are aligned
                while (d & 15)
                {
                    *(UINT32 *)d = *(const UINT32 *)s;
                    d += 4;
                    s += 4;
                    n -= 4;
                }
            }
        }

        if (s & 15)
        {
            // Misaligned source
            while (n >= 16)
            {
#ifdef LW_WINDOWS
                t = _mm_loadu_ps((const float *)s);
                _mm_store_ps((float *)d, t);
#else
                __asm__ __volatile__
                (
                    "movups (%0), %%xmm0\n"
                    "movaps %%xmm0, (%1)\n"
                    : "+r" (s), "+r" (d) : : "memory"
                );
#endif
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
#ifdef LW_WINDOWS
                t = _mm_load_ps((const float *)s);
                _mm_store_ps((float *)d, t);
#else
                __asm__ __volatile__
                (
                    "movaps (%0), %%xmm0\n"
                    "movaps %%xmm0, (%1)\n"
                    : "+r" (s), "+r" (d) : : "memory"
                );
#endif
                d += 16;
                s += 16;
                n -= 16;
            }
        }
    }
    else
    {
        // No SSE, do a normal loop
        while (n >= 16)
        {
            *(UINT32 *)(d+0)  = *(const UINT32 *)(s+0);
            *(UINT32 *)(d+4)  = *(const UINT32 *)(s+4);
            *(UINT32 *)(d+8)  = *(const UINT32 *)(s+8);
            *(UINT32 *)(d+12) = *(const UINT32 *)(s+12);
            d += 16;
            s += 16;
            n -= 16;
        }
    }
    while (n >= 4)
    {
        *(UINT32 *)d = *(const UINT32 *)s;
        d += 4;
        s += 4;
        n -= 4;
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
    return 0 != (s_StandardFeatures & CPU_STD_SSE);
}

void Cpu::MemSet(void *dst, UINT08 data, size_t n)
{
    uintptr_t d = (uintptr_t)dst;

    // Align dst to 4 bytes
    if (n >= (4 - (d & 3))) // do we have at least that many in total to copy?
    {
        // Write 1 byte at a time until we are aligned
        while (d & 3)
        {
            *(UINT08 *)d = data;
            d++;
            n--;
        }
    }

    // Will write at least 4 bytes at a time
    const UINT32 data4 =
        static_cast<UINT32>(data) |
        (static_cast<UINT32>(data) << 8) |
        (static_cast<UINT32>(data) << 16) |
        (static_cast<UINT32>(data) << 24);

    if (s_StandardFeatures & CPU_STD_SSE)
    {
        // Align dst to 16 bytes
        if (n >= (16 - (d & 15))) // do we have at least that many in total to copy?
        {
            // Write 4 bytes at a time until we are aligned
            while (d & 15)
            {
                *(UINT32 *)d = data4;
                d += 4;
                n -= 4;
            }
        }

#ifdef LW_WINDOWS
        __m128 t;
        t = _mm_set_ps(*(float*)&data4, *(float*)&data4, *(float*)&data4, *(float*)&data4);
#else
        uintptr_t pdata4 = reinterpret_cast<uintptr_t>(&data4);
        __asm__ __volatile__
            (
             "movss (%0), %%xmm0\n"
             "unpcklps %%xmm0, %%xmm0\n"
             "movlhps %%xmm0, %%xmm0\n"
             : "+r" (pdata4) : : "memory"
            );
#endif
        while (n >= 16)
        {
#ifdef LW_WINDOWS
            _mm_store_ps((float *)d, t);
#else
            __asm__ __volatile__
                (
                 "movaps %%xmm0, (%0)\n"
                 : "+r" (d) : : "memory"
                );
#endif
            d += 16;
            n -= 16;
        }
    }
    else
    {
        // No SSE, do a normal loop
        while (n >= 16)
        {
            *(UINT32 *)(d+0)  = data4;
            *(UINT32 *)(d+4)  = data4;
            *(UINT32 *)(d+8)  = data4;
            *(UINT32 *)(d+12) = data4;
            d += 16;
            n -= 16;
        }
    }
    while (n >= 4)
    {
        *(UINT32 *)d = data4;
        d += 4;
        n -= 4;
    }
    while (n)
    {
        *(UINT08 *)d = data;
        d++;
        n--;
    }
}
