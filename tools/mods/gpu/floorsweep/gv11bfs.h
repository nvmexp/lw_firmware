/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#pragma once

#ifndef INCLUDED_GV11BFS_H
#define INCLUDED_GV11BFS_H

#include "gv10xfs.h"

class GV11BFs : public GV10xFs
{
public:
    GV11BFs(GpuSubdevice *pSubdev);
    virtual ~GV11BFs() {}

private:
    virtual UINT32 GetEngineResetMask();
};

#endif // INCLUDED_GV11BFS_H
