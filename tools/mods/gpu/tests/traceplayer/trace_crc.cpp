/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "trace_crc.h"
#include "trace_loader.h"
#include "core/include/utility.h"
#include "core/include/crc.h"

#include "class/cla097.h" // KEPLER_A
#include "class/cla197.h" // KEPLER_B
#include "class/cla297.h" // KEPLER_C
#include "class/clb097.h" // MAXWELL_A
#include "class/clb197.h" // MAXWELL_B
#include "class/clc097.h" // PASCAL_A
#include "class/clc197.h" // PASCAL_B
#include "class/clc397.h" // VOLTA_A
#include "class/clc597.h" // TURING_A
#include "class/clc697.h" // AMPERE_A
#include "class/clc797.h" // AMPERE_B
#include "class/clc997.h" // ADA_A
#include "class/clcb97.h" // HOPPER_A
#include "class/clcc97.h" // HOPPER_B
#include "class/clcd97.h" // BLACKWELL_A

UINT32 MfgTracePlayer::CalcCRC(const void* buf, UINT32 size)
{
    return OldCrc::Callwlate(const_cast<void*>(buf), size/4, 1, 32, size);
}

RC MfgTracePlayer::CRCChain::Load(Loader& loader, UINT32 classId)
{
    RC rc;
    m_Crcs.clear();

    CHECK_RC(GetChainForClass(classId, &m_Chain));

    string buffer;
    rc = loader.LoadFile("test.crc", &buffer);
    if (rc != RC::OK)
    {
        rc.Clear();
        rc = loader.LoadEncryptedFile("test.cre", &buffer);
        if (rc == RC::FILE_DOES_NOT_EXIST)
        {
            Printf(Tee::PriError, "File \"test.crc\" not found\n");
        }
        CHECK_RC(rc);
    }

    typedef vector<string> Lines;
    Lines lines;
    CHECK_RC(Utility::Tokenizer(buffer, "\r\n", &lines));

    bool foundTraceDx5Section = false;
    for (const auto& line : lines)
    {
        // First, find the trace_dx5 section
        if (!foundTraceDx5Section)
        {
            if (line.find("[trace_dx5]") != string::npos)
            {
                foundTraceDx5Section = true;
            }
            continue;
        }

        // Skip further sections
        if (line.find('[') != string::npos)
        {
            break;
        }

        // Split line into tokens
        vector<string> tokens;
        CHECK_RC(Utility::Tokenizer(line, " \t=", &tokens));

        // Save the CRC
        if (tokens.size() == 2)
        {
            char* endptr = nullptr;
            const UINT64 value = Utility::Strtoull(tokens[1].c_str(), &endptr, 0);
            const char* const expectedEnd = tokens[1].c_str() + tokens[1].size();
            if ((value >= 0xFFFFFFFFULL) || (endptr != expectedEnd))
            {
                Printf(Tee::PriLow, "Ignoring invalid CRC: \"%s\"\n", line.c_str());
            }
            else
            {
                m_Crcs[tokens[0]] = static_cast<UINT32>(value);
            }
        }
        else
        {
            Printf(Tee::PriLow, "Ignoring unrecognized CRC: \"%s\"\n", line.c_str());
        }
    }

    return rc;
}

RC MfgTracePlayer::CRCChain::GetCrc(const string& key, const string& suffix, UINT32* pCrc) const
{
    const char*const* c = m_Chain;
    for (; c && *c; c++)
    {
        const string crcKey = Utility::StrPrintf("%s_%s_0%s", *c, key.c_str(), suffix.c_str());
        const auto   crc    = m_Crcs.find(crcKey);
        if (crc != m_Crcs.end())
        {
            Printf(Tee::PriDebug, "Found key %s\n", crcKey.c_str());
            *pCrc = crc->second;
            return OK;
        }
        Printf(Tee::PriDebug, "Ignoring key %s\n", crcKey.c_str());
    }

    // Not sure why heaven_30 trace for kepler_c has this key
    const string crcKey = "gt212_copy_engine_" + key + "_0" + suffix;
    const auto   crc    = m_Crcs.find(crcKey);
    if (crc != m_Crcs.end())
    {
        *pCrc = crc->second;
        return OK;
    }

    Printf(Tee::PriError, "Unable to find CRC for key %s\n", crcKey.c_str());
    return RC::GOLDEN_VALUE_NOT_FOUND;
}

RC MfgTracePlayer::CRCChain::GetChainForClass(UINT32 classId, Chain* pChain)
{
    static const struct Chain
    {
        UINT32 classId;
        const char* const chain[10];
    }
    chains[] =
    {
        // from diag/mods/mdiag/resource/lwgpu/crcchain.cpp
        { KEPLER_A,     { "gk104", "kepler_a" } },
        { KEPLER_B,     { "gk20x", "gk20a", "gk110", "gk104", "kepler_a" } },
        { KEPLER_C,     { "gk20a", "gk110", "gk104", "kepler_a" } },
        { MAXWELL_A,    { "gm108", "maxwell_a", "gk20x", "gk20a", "gk110", "gk104", "kepler_a" } },
        { MAXWELL_B,    { "gm208", "gm108", "maxwell_a", "gk20x", "gk20a", "gk110", "gk104", "kepler_a" } },
        { PASCAL_A,     { "gp108", "pascal_a", "gm208", "gm108" } },
        { PASCAL_B,     { "gp108", "pascal_a", "gm208", "gm108" } },
        { VOLTA_A,      { "gv100", "volta_a", "gp108"}},
        { TURING_A,     { "g000", "tu100", "tu102", "gv100"} },
        { AMPERE_A,     { "ga108", "ampere_a", "tu108", "turing_a" } },
        { AMPERE_B,     { "ga108", "ampere_a", "tu108", "turing_a" } },
        { ADA_A,        { "ad108", "ada_a", "ga108", "ampere_a"    } },
        { HOPPER_A,     { "gh100", "hopper_a", "ga108", "ampere_a" } },
        { HOPPER_B,     { "gh100", "hopper_a", "ad108", "ada_a"} },
        { BLACKWELL_A,  { "gb100", "blackwell_a", "gh208", "gh100", "hopper_a"} },
    };

    for (UINT32 ic=0; ic < sizeof(chains)/sizeof(chains[0]); ic++)
    {
        if (chains[ic].classId == classId)
        {
            *pChain = chains[ic].chain;
            return OK;
        }
    }

    Printf(Tee::PriError, "No CRC chain found for class 0x%04x\n", classId);
    return RC::GOLDEN_VALUE_NOT_FOUND;
}
