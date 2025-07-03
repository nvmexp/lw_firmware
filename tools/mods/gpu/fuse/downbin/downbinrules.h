/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"

//!
//! \brief Some underlying rules / relationships between the floorsweepable elements
//!        ilwolved in downbinning
//!        This may either verify a given condition is met or modify the state of the
//!        floorsweeping config to match the rules
//!
class DownbinRule
{
public:
    DownbinRule() {};
    virtual ~DownbinRule() = default;

    virtual string GetName() const { return ""; }

    virtual RC Execute() { return RC::UNSUPPORTED_FUNCTION; }
};
