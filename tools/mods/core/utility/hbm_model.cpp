/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/hbm_model.h"

#include "core/include/massert.h"
#include "core/include/tee.h"
#include "core/include/utility.h"

using namespace Memory;

string Hbm::ToString(Hbm::SpecVersion specVersion)
{
    switch (specVersion)
    {
        case Hbm::SpecVersion::V2:  return "HBM2";
        case Hbm::SpecVersion::V2e: return "HBM2e";
        case Hbm::SpecVersion::All: return "all_hbm_specs";

        default:
            MASSERT(!"Unknown string colwersion for HBM spec version");
            Printf(Tee::PriWarn, "Unknown string colwersion for HBM spec version");
            // [[fallthrough]] @c++17
        case Hbm::SpecVersion::Unknown: return "unknown";
    }
}

string Hbm::ToString(Hbm::Vendor vendor)
{
    switch (vendor)
    {
        case Hbm::Vendor::Samsung: return "Samsung";
        case Hbm::Vendor::SkHynix: return "SK Hynix";
        case Hbm::Vendor::All:     return "all_vendors";

        default:
            MASSERT(!"Unknown string colwersion for HBM vendor");
            Printf(Tee::PriWarn, "Unknown string colwersion for HBM vendor");
            // [[fallthrough]] @c++17
        case Hbm::Vendor::Unknown: return "unknown";
    }
}

string Hbm::ToString(Hbm::Die die)
{
    switch (die)
    {
        case Hbm::Die::B:    return "B";
        case Hbm::Die::X:    return "X";
        case Hbm::Die::Mask: return "Mask";
        case Hbm::Die::All:  return "all";

        default:
            MASSERT(!"Unknown string colwersion for HBM die");
            Printf(Tee::PriWarn, "Unknown string colwersion for HBM die");
            // [[fallthrough]] @c++17
        case Hbm::Die::Unknown: return "unknown";
    }
}

string Hbm::ToString(Hbm::StackHeight stackHeight)
{
    switch (stackHeight)
    {
        case Hbm::StackHeight::Hi4: return "4HI";
        case Hbm::StackHeight::Hi8: return "8HI";
        case Hbm::StackHeight::All: return "all_stack_heights";

        default:
            MASSERT(!"Unknown string colwersion for HBM stack height");
            Printf(Tee::PriWarn, "Unknown string colwersion for HBM stack height");
            // [[fallthrough]] @c++17
        case Hbm::StackHeight::Unknown: return "unknown";
    }
}

string Hbm::ToString(Hbm::Revision rev)
{
    switch (rev)
    {
        case Hbm::Revision::A:   return "A";
        case Hbm::Revision::B:   return "B";
        case Hbm::Revision::C:   return "C";
        case Hbm::Revision::D:   return "D";
        case Hbm::Revision::E:   return "E";
        case Hbm::Revision::F:   return "F";
        case Hbm::Revision::A1:  return "A1";
        case Hbm::Revision::A2:  return "A2";
        case Hbm::Revision::V3:  return "3";
        case Hbm::Revision::V4:  return "4";
        case Hbm::Revision::All: return "all_revisions";

        default:
            MASSERT(!"Unknown string colwersion for HBM revision");
            Printf(Tee::PriWarn, "Unknown string colwersion for HBM revision");
            // [[fallthrough]] @c++17
        case Hbm::Revision::Unknown: return "unknown";
    }
}

string Hbm::Model::ToString() const
{
    return Utility::StrPrintf("%s %s %s %s %s", Hbm::ToString(spec).c_str(),
                              Hbm::ToString(vendor).c_str(), Hbm::ToString(die).c_str(),
                              Hbm::ToString(height).c_str(), Hbm::ToString(revision).c_str());
}
