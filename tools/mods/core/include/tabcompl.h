/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
// Tab completion for MODS interactive interface

#pragma once

#include "types.h"
#include <string>
#include <vector>

namespace TabCompletion
{
    // Find possible completions for given command (Input).
    // Completions will be stored in Result, and return value is string that
    // tab completion should insert into command, i.e.:
    // "CheetAh.R" -> "eg" // Result = ["RegRd", "RegWr"]
    string Complete(const string& Input, vector<string>* pResult);

    // Format tab completion help message. Result will contain lines not
    // containing EOL character.
    void Format(const vector<string>& Words, UINT32 Columns,
                                                       vector<string>* pResult);
}
