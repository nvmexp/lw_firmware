
/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016,2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_GV10xFS_H
#define INCLUDED_GV10xFS_H

#include "gp10xfs.h"

class GV10xFs : public GP10xFs
{
public:
    GV10xFs(GpuSubdevice *pSubdev) : GP10xFs(pSubdev) {}

    UINT32 SmMask(UINT32 HwGpcNum, UINT32 HwTpcNum) const override;
    UINT32 HalfFbpaMask() const override;
    UINT32  PceMask() const override;
protected:
    void ApplyFloorsweepingSettings() override;
    void RestoreFloorsweepingSettings() override;
    void ResetFloorsweepingSettings() override;
    void SaveFloorsweepingSettings() override;
    RC   CheckFloorsweepingArguments() override;
    RC   ReadFloorsweepingArgumentsImpl(ArgDatabase& fsArgDb) override;

private:
    UINT32 GetEngineResetMask() override;
};

#endif // INCLUDED_GV10xFS_H

