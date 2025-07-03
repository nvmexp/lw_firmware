/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

// Branch out AD10B from Hopper since it will use gen 5.
// Created for transition window during gen4 to 5, if no difference this file can be deleted.
#include "hopperpcie.h"

class AD10BPcie : public HopperPcie
{
public:
    AD10BPcie() = default;
    virtual ~AD10BPcie() = default;
protected:
    RC     DoGetScpmStatus(UINT32 * pStatus) const override;
};

