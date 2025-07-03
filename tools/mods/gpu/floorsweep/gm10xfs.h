/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2016,2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_GM10XFS_H
#define INCLUDED_GM10XFS_H

#include "fermifs.h"

class GpuSubdevice;

class GM10xFs : public FermiFs
{
public:
    GM10xFs(GpuSubdevice *pSubdev);
    virtual ~GM10xFs() {}

protected:
    virtual void    ApplyFloorsweepingSettings();
    virtual RC      GetFloorsweepingMasks(FloorsweepingMasks* pFsMasks) const;
    virtual RC      ReadFloorsweepingArgumentsImpl(ArgDatabase& fsArgDb);
    virtual void    RestoreFloorsweepingSettings();
    virtual void    ResetFloorsweepingSettings();
    virtual void    SaveFloorsweepingSettings();
    virtual void    PrintFloorsweepingParams();
    virtual RC      CheckFloorsweepingArguments();

    virtual UINT32  FbpMaskImpl() const;
    virtual UINT32  FbioMaskImpl() const;
    virtual UINT32  FbioshiftMaskImpl() const;
    virtual UINT32  FbpaMaskImpl() const;
    virtual UINT32  FscontrolMask() const;
    virtual UINT32  GpcMask() const;
    virtual UINT32  LwdecMask() const;
    virtual UINT32  LwencMask() const;
    virtual UINT32  SpareMask() const;
    virtual UINT32  TpcMask(UINT32 HwGpcNum) const;
    virtual UINT32  XpMask() const;
    virtual UINT32  ZlwllMask(UINT32 HwGpcNum) const;
    virtual void    RestoreFbio() const;
    virtual void    ResetFbio() const;
    virtual RC      PrintMiscFuseRegs();

protected:
    UINT32 m_SavedFsCeControl = 0;

    bool   m_FsLwencParamPresent = false;
    UINT32 m_SavedFsLwencControl = 0;
    UINT32 m_LwencMask = 0;

    bool m_FsLwdecParamPresent = false;
    UINT32 m_LwdecMask = 0;

private:
    UINT32 m_SavedFsDispControl = 0;
    UINT32 m_SavedFsLwdecControl = 0;
    UINT32 m_SavedFsFbioShiftOverrideControl = 0;
    UINT32 m_SavedFsPcieLaneControl = 0;

    bool m_FsHeadParamPresent = false;
    UINT32 m_HeadMask = 0;
    UINT32 m_SavedHead = 0;
};

#endif // INCLUDED_GM10XFS_H
