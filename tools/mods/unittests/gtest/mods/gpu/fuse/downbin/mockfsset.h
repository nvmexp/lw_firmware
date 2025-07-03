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

#include "gpu/fuse/downbin/fsset.h"

//------------------------------------------------------------------------------
class MockFsSet : public FsSet
{
    using FsUnit   = Downbin::FsUnit;
    using Settings = Downbin::Settings;

public:
    MockFsSet(FsUnit groupUnit, UINT32 numGroups)
        : FsSet(groupUnit, numGroups)
    {}

    const Settings* GetDownbinSettings() const
    {
        return &m_DownbinSettings;
    }

    Settings m_DownbinSettings;
};
