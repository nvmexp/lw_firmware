/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "random.h"
#include "core/include/types.h"
#include "core/include/rc.h"

// LwLink interface encapsulating FLA functionality
class LwLinkFla
{
public:
    virtual ~LwLinkFla() { }

    bool GetFlaCapable()
        { return DoGetFlaCapable(); }
    bool GetFlaEnabled()
        { return DoGetFlaEnabled(); }

private:
    virtual bool DoGetFlaCapable() = 0;
    virtual bool DoGetFlaEnabled() = 0;

};
