/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016,2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDE_GV100_FUSE_H
#define INCLUDE_GV100_FUSE_H

#include "gp10x_fuse.h"

class GV100Fuse : public GP10xFuse
{
public:
    GV100Fuse(Device *pDevice);
    bool IsFusePrivSelwrityEnabled() override;
    virtual RC GetFuseWord(UINT32 FuseRow, FLOAT64 TimeoutMs, UINT32 *pRetData) override;

protected:
    RC CheckFuseblowEnabled() override;
};

#endif

