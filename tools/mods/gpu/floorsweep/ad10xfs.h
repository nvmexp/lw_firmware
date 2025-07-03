/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "ga10xfs.h"

class AD10xFs : public GA10xFs
{
public:
    explicit AD10xFs(GpuSubdevice *pSubdev);
    virtual ~AD10xFs() {}

protected:
    RC      InitializePriRing_FirstStage(bool bInSetup) override;
    RC      InitializePriRing() override;
};
