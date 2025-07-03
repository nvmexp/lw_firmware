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

#pragma once

#include "core/include/errormap.h"
#include "core/include/memtypes.h"

namespace Repair
{
    struct RowError
    {
        Memory::Row row;                      //!< Error location information
        INT32 originTestId = s_UnknownTestId; //!< Test that found the error
    };
}
