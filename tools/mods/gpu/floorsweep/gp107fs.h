/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_GP107FS_H
#define INCLUDED_GP107FS_H

#ifndef INCLUDED_GP10xFS_H
#include "gp10xfs.h"
#endif

class GP107Fs : public GP10xFs
{
public:
    GP107Fs(GpuSubdevice *pSubdev);
    UINT32 HalfFbpaMask() const override;

protected:
    void ApplyFloorsweepingSettings() override;
    void RestoreFloorsweepingSettings() override;
    void ResetFloorsweepingSettings() override;
    void SaveFloorsweepingSettings() override;

private:
};

#endif // INCLUDED_GP107FS_H

