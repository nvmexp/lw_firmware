/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2008 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  surfcomp.h
 * @brief Utilities for reading compression tags.
 */

#ifndef INCLUDED_SURFCOMP_H
#define INCLUDED_SURFCOMP_H

#include "core/include/rc.h"

#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif
#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

class Surface2D;
typedef Surface2D Surface;

namespace SurfaceUtils {

enum ZBCComprType
{
    zbc_uncompressed,
    zbc_c1,
    zbc_c2,
    zbc_c3
};

struct ZBCValue
{
    enum MaxClearValues {
        maxClearValues = 4
    };
    ZBCComprType comprType;
    UINT32 clearValue[maxClearValues];
};

bool IsZBCValueEqual(const ZBCValue& v1, const ZBCValue& v2);

RC ClearCompTags
(
    Surface& Surf,
    UINT32 MinTag,
    UINT32 MaxTag,
    const ZBCValue& ClearValue
);

ZBCComprType GetZbcComprType(UINT32 PteKind);
ZBCComprType GetReductionComprType(UINT32 PteKind);

} // namespace SurfaceUtils

#endif // INCLUDED_SURFCOMP_H
