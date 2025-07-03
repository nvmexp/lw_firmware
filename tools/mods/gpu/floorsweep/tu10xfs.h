/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_TU10XFS_H
#define INCLUDED_TU10XFS_H

#include "gv10xfs.h"
#include "ctrl/ctrl90e7.h"

class TU10xFs : public GV10xFs
{
public:
    explicit TU10xFs(GpuSubdevice *pSubdev) : GV10xFs(pSubdev) {}
    virtual ~TU10xFs() {}

    UINT32  LwLinkMask() const override;
protected:
    bool    IsFloorsweepingAllowed() const override;
    void    ApplyFloorsweepingSettings() override;
    RC      PreserveFloorsweepingSettings() const override;
    void    RestoreFloorsweepingSettings() override;
    void    ResetFloorsweepingSettings() override;
    void    SaveFloorsweepingSettings() override;
    RC      ReadFloorsweepingArgumentsImpl(ArgDatabase& fsArgDb) override;
    void    PrintFloorsweepingParams() override;
    RC      PrintPreservedFloorsweeping() const override;
    UINT32  GetHalfFbpaWidth() const override;

    virtual bool IsIfrFsEntry(UINT32 address) const;
    virtual RC PopulateFsInforom(LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS *pIfrParams) const;
    virtual RC PopulateFsInforomEntry
    (
        LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS *pIfrParams,
        UINT32 address,
        UINT32 data,
        UINT32 maxIfrDataCount
    ) const;

    void    FbEnable(bool bEnable) override;

    RC SwReset(bool bReinitPriRing) override;

    bool   m_FsLwlinkParamPresent = false;
    UINT32 m_SavedFsLwlinkControl = 0;
    UINT32 m_LwlinkMask = 0;

    bool   m_FsHalfLtcParamPresent = false;

private:
    UINT32 GetEngineResetMask() override;
};

#endif // INCLUDED_TU10XFS_H

