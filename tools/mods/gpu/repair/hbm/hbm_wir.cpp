/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * `LWIDIA_COPYRIGHT_END
 */

#include "hbm_wir.h"

#include "core/include/massert.h"
#include "core/include/tee.h"
#include "core/include/utility.h"

#include <sstream>

using namespace Memory;

namespace
{
    template<typename T>
    void AddSupportSetToString(string* pStr, Hbm::SupportSet<T> supportSet)
    {
        MASSERT(pStr);

        if (supportSet.IsAllSupported())
        {
            *pStr += "all";
        }
        else
        {
            for (const auto& item : supportSet)
            {
                *pStr += ToString(item) + ", ";
            }

            // Remove trailing seperator
            if (!pStr->empty()) {
                pStr->pop_back();
                pStr->pop_back();
            }
        }
    }
}

/******************************************************************************/
// SupportedHbmModels

Hbm::SupportedHbmModels::SupportedHbmModels(std::initializer_list<SpecVersion> specs,
                                            std::initializer_list<Vendor> vendors,
                                            std::initializer_list<Die> dies,
                                            std::initializer_list<StackHeight> stackHeights,
                                            std::initializer_list<Revision> revisions)
    : m_SpecVersions(specs), m_Vendors(vendors), m_Dies(dies),
      m_StackHeights(stackHeights), m_Revisions(revisions)
{}

Hbm::SupportedHbmModels::SupportedHbmModels(const SupportedHbmModels& o)
    : m_SpecVersions(o.m_SpecVersions),
      m_Vendors(o.m_Vendors),
      m_Dies(o.m_Dies),
      m_StackHeights(o.m_StackHeights),
      m_Revisions(o.m_Revisions)
{}

Hbm::SupportedHbmModels::SupportedHbmModels(SupportedHbmModels&& o)
    : m_SpecVersions(std::move(o.m_SpecVersions)),
      m_Vendors(std::move(o.m_Vendors)),
      m_Dies(std::move(o.m_Dies)),
      m_StackHeights(std::move(o.m_StackHeights)),
      m_Revisions(std::move(o.m_Revisions))
{}

bool Hbm::SupportedHbmModels::IsSupported(const Model& hbmModel) const
{
    return m_SpecVersions.IsSupported(hbmModel.spec)
        && m_Vendors.IsSupported(hbmModel.vendor)
        && m_Dies.IsSupported(hbmModel.die)
        && m_StackHeights.IsSupported(hbmModel.height)
        && m_Revisions.IsSupported(hbmModel.revision);
}

string Hbm::SupportedHbmModels::ToString() const
{
    string s;

    s += "SpecVersions{";
    AddSupportSetToString(&s, m_SpecVersions);
    s += "}";

    s += ", Vendors{";
    AddSupportSetToString(&s, m_Vendors);
    s += "}";

    s += ", Dies{";
    AddSupportSetToString(&s, m_Dies);
    s += "}";

    s += ", StackHeights{";
    AddSupportSetToString(&s, m_StackHeights);
    s += "}";

    s += ", Revisions{";
    AddSupportSetToString(&s, m_Revisions);
    s += "}";

    return s;
}

/******************************************************************************/
// Wir

Hbm::Wir::Wir
(
    const char* name,
    Type wirType,
    InstrEncoding instr,
    RegType regType,
    UINT32 wdrLength,
    const SupportedHbmModels& supportedHbmModels,
    Flags flags
) : m_name(name),
    m_type(wirType),
    m_instr(instr),
    m_regType(regType),
    m_wdrLength(wdrLength),
    m_flags(flags),
    m_SupportedHbmModels(supportedHbmModels)
{}

Hbm::Wir::Wir
(
    const char* name,
    Type wirType,
    InstrEncoding instr,
    RegType regType,
    UINT32 wdrLength,
    const SupportedHbmModels& supportedHbmModels
) : Wir(name, wirType, instr, regType, wdrLength, supportedHbmModels, Flags::None) {}

UINT32 Hbm::Wir::Encode(ChannelSelect chanSel) const
{
    MASSERT(chanSel != CHANNEL_SELECT_ILWALID && "channel select is not set");
    MASSERT((chanSel & ~0x0f) == 0 && "channel select value is larger than 4 bits");

    if ((m_flags & Flags::SingleChannelOnly) == Flags::SingleChannelOnly
        && chanSel == Wir::CHANNEL_SELECT_ALL)
    {
        Printf(Tee::PriError, "WIR only supports single channel operation:\n\tWIR: %s\n",
               FullToString().c_str());
        MASSERT(!"WIR only spports single channel operation");
    }

    return static_cast<UINT32>(chanSel) << 8 // channel: bits [11:8]
        | m_instr;                           // instr  : bits [7:0]
}

string Hbm::Wir::ToString(RegType regType)
{
    string s;

    if ((regType & RegType::Read) != 0)  { s += "Read|"; }
    if ((regType & RegType::Write) != 0) { s += "Write|"; }

    // Remove trailing separator
    if (!s.empty()) { s.pop_back(); }

    return s;
}

string Hbm::Wir::FullToString() const
{
    // TODO(stewarts): stringify flags
    return Utility::StrPrintf("Name(%s) Opcode(%X), RegType(%s), WdrLen(%u), HbmModels(%s)",
                              m_name.c_str(), m_instr, ToString(m_regType).c_str(),
                              m_wdrLength, m_SupportedHbmModels.ToString().c_str());
}

/******************************************************************************/
// WdrData

string Hbm::WdrData::ToString() const
{
    stringstream ss;
    ss << "{";

    for (UINT32 e : m_Data)
    {
        ss << e << ", ";
    }

    string s = ss.str();

    if (!m_Data.empty())
    {
        // Remove seperator
        s.pop_back();
        s.pop_back();
    }

    s += "}";

    return s;
}
