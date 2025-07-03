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

#include "core/utility/obfuscate_string.h"

namespace
{
    //----------------------------------------------------------------
    //! \brief Run-time random number generator
    //!
    //! This RNG generates the same random sequence as
    //! Obfuscate::CtPcgRandom.  It is only used by
    //! Obfuscate::RtString, so need not be exposed to the global
    //! namespace.
    //!
    class RtPcgRandom : public Obfuscate::BasePcgRandom
    {
    protected:
        UINT64 m_State;
        UINT64 m_Stream;
    public:
        explicit RtPcgRandom(UINT32 seed);
        UINT32 GetRandom();
    };
}

//--------------------------------------------------------------------
RtPcgRandom::RtPcgRandom(UINT32 seed)
{
    const UINT64 genStream = (static_cast<UINT64>(seed) << 1) + 1;

    const UINT64 genState0 = 1;
    const UINT64 genState1 = GetNextState(genState0, genStream);
    const UINT64 genState2 = GetNextState(genState1, genStream);
    const UINT64 genState3 = GetNextState(genState2, genStream);
    const UINT64 genState4 = GetNextState(genState3, genStream);
    const UINT64 genState5 = GetNextState(genState4, genStream);
    const UINT64 genState6 = GetNextState(genState5, genStream);
    const UINT64 genState7 = GetNextState(genState6, genStream);

    const UINT32 genSeed0 = (GetValue(genState0) +
                             GetValue(genState2));
    const UINT32 genSeed1 = (GetValue(genState1) +
                             GetValue(genState3));
    const UINT32 genSeed4 =  GetValue(genState6);
    const UINT32 genSeed5 =  GetValue(genState7);

    const UINT64 initState  = (static_cast<UINT64>(genSeed0) << 32) + genSeed4;
    const UINT64 initStream = (static_cast<UINT64>(genSeed1) << 32) + genSeed5;

    m_Stream = (initStream << 1) + 1;
    m_State  = GetNextState(initState + m_Stream, m_Stream);
}

//--------------------------------------------------------------------
//! \brief Return the next random number from 0x0 to 0xffffffff
//!
UINT32 RtPcgRandom::GetRandom()
{
    const UINT32 value = GetValue(m_State);
    m_State = GetNextState(m_State, m_Stream);
    return value;
}

//--------------------------------------------------------------------
//! \brief Decode a string that was encrypted by the CtString constructor
//!
string Obfuscate::RtString::Decode(UINT32 seed) const
{
#if __cplusplus >= 201402L
    string decodedString(m_String, m_Size);
#else
    const char *pData = reinterpret_cast<const char*>(&m_Size) + sizeof(m_Size);
    string decodedString(pData, m_Size);
#endif
    RtPcgRandom rng(seed);

    for (size_t ii = 0; ii < m_Size; ++ii)
    {
        decodedString[ii] ^= static_cast<char>(rng.GetRandom());
    }
    return decodedString;
}
