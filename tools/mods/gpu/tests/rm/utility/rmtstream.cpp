/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "rmtstring.h"
#include "rmtstream.h"

// File name and line number (location) if applicable
rmt::String rmt::FileLocation::locationString() const
{
    ostringstream oss;
    if (!file.empty())
    {
        oss << file;
        if (line)
            oss << '(' << line << ')';
        oss << ": ";
    }

    return oss.str();
}

