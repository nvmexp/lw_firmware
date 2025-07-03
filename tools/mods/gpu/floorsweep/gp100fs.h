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

#ifndef INCLUDED_GP100FS_H
#define INCLUDED_GP100FS_H

#include "gp10xfs.h"

class GP100Fs : public GP10xFs
{
public:
    GP100Fs(GpuSubdevice *pSubdev);
    virtual ~GP100Fs() {}

private:
    virtual UINT32 GetEngineResetMask();
};

#endif // INCLUDED_GP100FS_H
