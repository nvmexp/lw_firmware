/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "fusetypes.h"

map<FuseMacroType, string> fuseMacroString =
{
    { FuseMacroType::Fuse, "FUSE" },
    { FuseMacroType::Fpf, "FPF" },
    { FuseMacroType::Rir, "RIR" }
};

OldFuse::Tolerance FuseFilterToTolerance(FuseFilter filter)
{
    UINT32 tolerance = OldFuse::REDUNDANT_MATCH;
    if ((filter & FuseFilter::LWSTOMER_FUSES_ONLY) != 0)
    {
        tolerance |= OldFuse::LWSTOMER_MATCH;
    }
    return static_cast<OldFuse::Tolerance>(tolerance);
}

FuseFilter ToleranceToFuseFilter(OldFuse::Tolerance tolerance)
{
    FuseFilter filter = FuseFilter::ALL_FUSES;
    if ((tolerance & OldFuse::LWSTOMER_MATCH) != 0)
    {
        filter |= FuseFilter::LWSTOMER_FUSES_ONLY;
    }
    return filter;
}
