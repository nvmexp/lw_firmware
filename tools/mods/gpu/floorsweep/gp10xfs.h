/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2016,2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_GP10xFS_H
#define INCLUDED_GP10xFS_H

#ifndef INCLUDED_GM20xFS_H
#include "gm20xfs.h"
#endif

class GP10xFs : public GM20xFs
{
public:
    GP10xFs(GpuSubdevice *pSubdev);
    virtual ~GP10xFs() {}

protected:
    virtual void    ApplyFloorsweepingSettings();
    virtual void    ApplyFloorsweepingSettingsInternal();
    virtual RC      CheckFloorsweepingArguments();
    virtual RC      GetFloorsweepingMasks(FloorsweepingMasks* pFsMasks) const;
    virtual void    PrintFloorsweepingParams();
    virtual RC      ReadFloorsweepingArgumentsImpl(ArgDatabase& fsArgDb);
    virtual void    RestoreFloorsweepingSettings();
    virtual void    ResetFloorsweepingSettings();
    virtual void    SaveFloorsweepingSettings();
    virtual RC      PrintMiscFuseRegs();
    virtual UINT32  FbioMaskImpl() const ;
    virtual UINT32  FbpMaskImpl() const;
    virtual UINT32  FbpaMaskImpl() const;
    virtual UINT32  PesMask(UINT32 HwGpcNum) const;
    virtual UINT32  LwencMask() const;
    virtual bool    IsCeFloorsweepingSupported() const { return false; }
    virtual void    CheckIfPriRingInitIsRequired();
    virtual UINT32  GetHalfFbpaWidth() const;
    
    virtual RC      AdjustDependentFloorsweepingArguments();
    virtual RC      AdjustGpcFloorsweepingArguments();
    virtual RC      AdjustPreGpcFloorsweepingArguments(UINT32 *pModifiedTpcMask);
    virtual RC      AdjustPesFloorsweepingArguments(UINT32 *pModifiedTpcMask);
    virtual RC      AdjustPostGpcFloorsweepingArguments();
    virtual RC      AdjustFbpFloorsweepingArguments();

    bool   m_FsHalfFbpaParamPresent;
    UINT32 m_HalfFbpaMask;
    UINT32 m_SavedHalfFbpaMask;

    vector<bool>   m_FsGpcPesParamPresent;
    vector<UINT32> m_GpcPesMasks;

    bool   m_HbmParamPresent;
    UINT32 m_HbmMask;
private:
    vector<UINT32> m_SavedGpcPesMasks;

    virtual void    CheckFbioCount();
    virtual UINT32 GetEngineResetMask();
};

#endif // INCLUDED_GM20xGPU_H

