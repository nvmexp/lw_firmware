/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation. All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/tests/modstest.h"

class CpuStress : public ModsTest
{
public:
    virtual ~CpuStress() = default;

protected:
    explicit CpuStress()
    {
        SetName("CpuStress");
    }
};

unique_ptr<CpuStress> MakeCpuStress();
