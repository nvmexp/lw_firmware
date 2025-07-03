/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_OBFUSCATE_H
#define INCLUDED_OBFUSCATE_H

#include "core/include/types.h"
#include <string>
#include <array>
#include <cstdarg>

//--------------------------------------------------------------------
// This file helps obfuscate strings that we need to encrypt in our
// code to avoid distributing IP.  It's less secure than the
// REDACT_RANDOMS mechanism in the Makefile (which searches the binary
// for a set of strings), but easier to maintain for cases in which a
// file contains a large number of strings that would be diffilwlt to
// maintain in tools/redact_list.
//
// The encryption mechanism is to XOR the characters in the string
// with random numbers.  The encryption is done at compile time with
// templates.  The decryption is done at run time with a non-templated
// functions, so that we don't crowd the run-time code with a zillion
// templated functions.
//
// Sample code to encrypt a string "foo" at compile time with seed 0x12345:
//     constexpr UINT32 seed = 0x12345;
//     Obfuscate::CtString<seed, sizeof("foo")> encryptedString("foo");
//
// Sample code to decrypt the above string at run time:
//     Printf(Tee::PriNormal, "%s\n", encryptedString.Decode(seed));
//
namespace Obfuscate
{
    #ifdef LW_WINDOWS
    // Suppress the Windows "integral constant overflow" error.
    // This code relies on integer overflows, especially in the
    // compile-time LCG RNG.
    #pragma warning(push)
    #pragma warning(disable:4307)
    #endif

    //----------------------------------------------------------------
    //! \brief Base class for random-number generators
    //!
    //! Base class for CtPcgRandom (compile-time RNG) and RtPcgRandom
    //! (run-time RNG).  Given a 32-bit seed, both subclasses generate
    //! the same sequence of 32-bit random numbers from 0x0 to
    //! 0xffffffff.
    //!
    //! The algorithms are heavily based on the PCG algorithm in
    //! //sw/dev/gpu_drv/chips_a/diag/utils/random.h, and at the time
    //! of this writing (Sep 27, 2020), they generate the exact same
    //! sequence of random numbers as Random::GetRandom() given the
    //! same 32-bit seed.
    //!
    class BasePcgRandom
    {
    protected:
        static constexpr UINT64 GetNextState(UINT64 state, UINT64 stream)
        {
            return state * s_Multiplier + stream;
        }
        static constexpr UINT32 GetValue(const UINT64 state)
        {
            return ((XorShifted(state) >> Rot(state)) |
                    (XorShifted(state) << ((-Rot(state)) & 31)));
        }
    private:
        static constexpr UINT64 s_Multiplier = 0x5851f42d4c957f2dULL;
        static constexpr UINT32 XorShifted(UINT64 state)
        {
            return static_cast<UINT32>(((state >> 18) ^ state) >> 27);
        }
        static constexpr int Rot(UINT64 state)
        {
            return static_cast<int>(state >> 59);
        }
    };

    //----------------------------------------------------------------
    //! \brief Compile-time random number generator
    //!
    //! Here's some sample code that demonstrates how to use this
    //! class to generate a sequence of random numbers at compile
    //! time:
    //!
    //!     constexpr UINT32 seed = <some number>;
    //!     constexpr UINT32 randomNumbers[] = 
    //!     {
    //!         CtPcgRandom<seed, 0>::value,
    //!         CtPcgRandom<seed, 1>::value,
    //!         CtPcgRandom<seed, 2>::value,
    //!         CtPcgRandom<seed, 3>::value,
    //!         ...
    //!     };
    //!
    template<UINT32 SEED, size_t INDEX> class CtPcgRandom : public BasePcgRandom
    {
    public:
        static constexpr UINT64 stream = CtPcgRandom<SEED, INDEX-1>::stream;
        static constexpr UINT64 state =
            GetNextState(CtPcgRandom<SEED, INDEX-1>::state, stream);
        static constexpr UINT32 value = GetValue(state);
    };

    template<UINT32 SEED> class CtPcgRandom<SEED, 0> : public BasePcgRandom
    {
    private:
        // These private members are used to generate the initial
        // stream & state of the RNG, so that similar seeds generate
        // different random sequences.  They mimic the algorithm of
        // Random::SeedRandom() called with a single 32-bit seed.
        //
        static constexpr UINT64 genStream =
            (static_cast<UINT64>(SEED) << 1) + 1;

        static constexpr UINT64 genState0 = 1;
        static constexpr UINT64 genState1 = GetNextState(genState0, genStream);
        static constexpr UINT64 genState2 = GetNextState(genState1, genStream);
        static constexpr UINT64 genState3 = GetNextState(genState2, genStream);
        static constexpr UINT64 genState4 = GetNextState(genState3, genStream);
        static constexpr UINT64 genState5 = GetNextState(genState4, genStream);
        static constexpr UINT64 genState6 = GetNextState(genState5, genStream);
        static constexpr UINT64 genState7 = GetNextState(genState6, genStream);

        static constexpr UINT32 genSeed0 = (GetValue(genState0) +
                                            GetValue(genState2));
        static constexpr UINT32 genSeed1 = (GetValue(genState1) +
                                            GetValue(genState3));
        static constexpr UINT32 genSeed2 =  GetValue(genState4);
        static constexpr UINT32 genSeed3 =  GetValue(genState5);
        static constexpr UINT32 genSeed4 =  GetValue(genState6);
        static constexpr UINT32 genSeed5 =  GetValue(genState7);

        static constexpr UINT64 initState =
            (static_cast<UINT64>(genSeed0) << 32) + genSeed4;
        static constexpr UINT64 initStream =
            (static_cast<UINT64>(genSeed1) << 32) + genSeed5;

    public:
        static constexpr UINT64 stream = (initStream << 1) + 1;
        static constexpr UINT64 state = GetNextState(initState + stream,
                                                     stream);
        static constexpr UINT32 value = GetValue(state);
    };

    //----------------------------------------------------------------
    // \brief Run-time encrypted string
    //
    // This is the base class for CtString, the templated class that
    // encrypts a string at compile time.  The base class can decrypt
    // at run time without templates.
    //
    class RtString
    {
    private:
#if __cplusplus >= 201402L
        // In C++14, we can pass a pointer to CtString::m_Data in the
        // constructor.  That doesn't work in older C++ (gcc 4.92), so
        // we assume m_Data comes right after m_Size.
        const char* m_String;
#endif
        size_t m_Size;
    public:
#if __cplusplus >= 201402L
        constexpr RtString(const char* str, size_t size) :
            m_String(str), m_Size(size) {}
#else
        explicit constexpr RtString(size_t size) : m_Size(size) {}
#endif
        string Decode(UINT32 seed) const;
    };

    //----------------------------------------------------------------
    // \brief Compile-time encrypted string
    //
    // Typical usage:
    //     #define STRING "<some string to encrypt>"
    //     constexpr UINT32 seed = <some number>
    //     Obfuscate::CtString<seed, sizeof(STRING)> encryptedString(STRING);
    //
    template<UINT32 SEED, size_t SIZE> class CtString: public RtString
    {
    private:
        char m_Data[SIZE];

        template<size_t INDEX> static constexpr char EncodeChar(const char* str)
        {
            return static_cast<char>(CtPcgRandom<SEED, INDEX>::value ^
                                     str[INDEX]);
        };

        template<size_t... INDEXES> constexpr CtString
        (
            const char* str,
            index_sequence<INDEXES...>
        ) :
#if __cplusplus >= 201402L
            RtString(&m_Data[0], SIZE),
#else
            RtString(SIZE),
#endif
            m_Data{EncodeChar<INDEXES>(str)...}
        {
        }

    public:
        explicit constexpr CtString(const char* str) :
            CtString(str, make_index_sequence<SIZE>())
        {
        }
    };

    #ifdef LW_WINDOWS
    #pragma warning(pop)
    #endif
} // namespace Obfuscate

#endif // INCLUDED_OBFUSCATE_H
