/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2016,2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_GM20xFS_H
#define INCLUDED_GM20xFS_H

#ifndef INCLUDED_GM10xFS_H
#include "gm10xfs.h"
#endif

class GM20xFs : public GM10xFs
{
public:
    GM20xFs(GpuSubdevice *pSubdev);
    virtual ~GM20xFs() {}

protected:
    virtual void    ApplyFloorsweepingSettings();
    virtual RC      GetFloorsweepingMasks(FloorsweepingMasks* pFsMasks) const;
    virtual void    PrintFloorsweepingParams();
    virtual RC      ReadFloorsweepingArgumentsImpl(ArgDatabase& fsArgDb);
    virtual void    RestoreFloorsweepingSettings();
    virtual void    ResetFloorsweepingSettings();
    virtual void    SaveFloorsweepingSettings();
    virtual RC      PrintMiscFuseRegs();
    virtual UINT32  FbpMaskImpl() const;
    virtual UINT32  FbioMaskImpl() const;
    virtual UINT32  L2MaskImpl(UINT32 HwFbpNum) const;
    virtual UINT32  LwencMask() const;
    virtual void    CheckIfPriRingInitIsRequired();
    virtual RC      SwReset(bool bReinitPriRing);

    vector<bool>   m_FsFbpL2ParamPresent;
    vector<UINT32> m_FbpL2Masks;

private:
    vector<UINT32> m_SavedFbpL2Masks;

    virtual UINT32 GetEngineResetMask();
};

#endif // INCLUDED_GM20xGPU_H

