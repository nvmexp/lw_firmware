/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/*
 * Fermi implementation of floorsweeping. This class supports floorsweeping for
 * the following GPUs:
 * GF100, GF104, GF106, GF108, GF110, GF117, GF118, GF119,
*/
#ifndef INCLUDED_FERMIFS_H
#define INCLUDED_FERMIFS_H

#include "gpu/include/floorsweepimpl.h"
#include "gpu/include/gpusbdev.h"

class FermiFs : public FloorsweepImpl
{
public:
    FermiFs(GpuSubdevice *pSubdev);
    virtual ~FermiFs() {}

protected:
    virtual void    SetVbiosControlsFloorsweep(bool bControlsFs);
    virtual RC      ApplyFloorsweepingChanges(bool InPreVBIOSSetup);
    virtual void    ApplyFloorsweepingSettings();
    virtual void    ApplyFloorsweepingSettingsFermi();
    RC              CheckFloorsweepingMask(UINT32 Allowed, UINT32 Requested, UINT32 Max, const string &Region) const;
    virtual void    CheckFbioCount();
    virtual RC      CheckFloorsweepingArguments();
    virtual UINT32  FbioMaskImpl() const;
    virtual UINT32  FbioshiftMaskImpl() const;
    virtual UINT32  FbMaskImpl() const;
    virtual UINT32  FbpMaskImpl() const;
    virtual UINT32  L2MaskImpl(UINT32 HwFbpNum) const;
    virtual UINT32  FbpaMaskImpl() const;
    virtual UINT32  FscontrolMask() const;
    virtual RC      GetFloorsweepingMasks(FloorsweepingMasks* pFsMasks) const;
    virtual RC      GetFloorsweepingMasksFermi(FloorsweepingMasks* pFsMasks) const;
    virtual UINT32  GetGpcEnableMask() const;
    UINT32          GetSavedFsControl() { return m_SavedFsControl; }
    virtual void    GetTpcEnableMaskOnGpc(vector<UINT32> *pValues) const;
    virtual void    GetZlwllEnableMaskOnGpc(vector<UINT32> *pValues) const;
    virtual UINT32  GpcMask() const;
    virtual UINT32  GfxGpcMask() const;
    virtual RC      InitializePriRing_FirstStage(bool bInSetup);
    virtual RC      InitializePriRing();
    virtual void    PostRmShutdown(bool bInPreVBIOSSetup);
    virtual void    PrintFloorsweepingParamsFermi() const;
    virtual void    PrintFloorsweepingParams();
    virtual RC      ReadFloorsweepingArguments();
    virtual RC      AdjustDependentFloorsweepingArguments();
    virtual RC      AddLwrFsMask();
    virtual RC      ReadFloorsweepingArgumentsCommon(ArgDatabase& fsArgDatabase);
    virtual RC      ReadFloorsweepingArgumentsGF108Plus(ArgDatabase& fsArgDatabase);
    virtual RC      ReadFloorsweepingArgumentsImpl(ArgDatabase& argDb) { return OK; }
    virtual void    CheckIfPriRingInitIsRequired();
    virtual void    RestoreFloorsweepingSettings();
    virtual void    RestoreFloorsweepingSettingsFermi();
    virtual void    ResetFloorsweepingSettings();
    virtual void    ResetFloorsweepingSettingsFermi();
    virtual void    RestoreFbio() const;
    virtual void    SaveFloorsweepingSettingsFermi();
    virtual void    SaveFloorsweepingSettings();
    void            SetSavedFsControl(UINT32 control) { m_SavedFsControl = control; }
    virtual UINT32  SmMask(UINT32 HwGpcNum, UINT32 HwTpcNum) const;
    virtual UINT32  SpareMask() const;
    virtual UINT32  TpcMask(UINT32 HwGpcNum) const;
    virtual UINT32  XpMask() const;
    virtual UINT32  ZlwllMask(UINT32 HwGpcNum) const;
    virtual UINT32  LwdecMask() const;
    virtual UINT32  LwencMask() const;
    virtual void    FbEnable(bool bEnable);
    virtual void    GraphicsEnable(bool bEnable);

    // Saved floorsweeping values, used to restore GPU to startup FS state at
    // mods shutdown time
    //
    UINT32 m_SavedFsControl = 0;
    UINT32 m_SavedGpcMask = 0;
    UINT32 m_SavedFbpMask = 0;
    vector< UINT32 > m_SavedGpcTpcMask;
    vector< UINT32 > m_SavedGpcZlwllMask;
    UINT32 m_SavedFbio = 0;
    UINT32 m_SavedFbioShift = 0;
    UINT32 m_SavedFbpa = 0;
    UINT32 m_SavedSpare = 0;

    // Mask settings the user requested on the command line
    //
    UINT32 m_FbMask = 0;
    UINT32 m_GpcMask = 0;
    vector<UINT32> m_GpcTpcMasks;
    vector<UINT32> m_GpcZlwllMasks;
    UINT32 m_DisplayMask = 0;
    UINT32 m_MsdecMask = 0;
    UINT32 m_MsvldMask = 0;
    UINT32 m_FbioShiftOverrideMask = 0;
    UINT32 m_CeMask = 0;
    UINT32 m_FbioMask = 0;
    UINT32 m_FbioShiftMask = 0;
    UINT32 m_PcieLaneMask = 0;
    UINT32 m_FbpaMask = 0;
    UINT32 m_SpareMask = 0;

    // Parameter present flags for the command line settings.
    bool m_FsDisplayParamPresent = false;
    bool m_FsMsdecParamPresent = false;
    bool m_FsMsvldParamPresent = false;
    bool m_FsFbioShiftOverrideParamPresent = false;
    bool m_FsCeParamPresent = false;
    bool m_FsGpcParamPresent = false;
    vector<bool> m_FsGpcTpcParamPresent;
    vector<bool> m_FsGpcZlwllParamPresent;
    bool m_FsFbParamPresent = false;
    bool m_FsFbioParamPresent = false;
    bool m_FsFbioShiftParamPresent = false;
    bool m_FsPcieLaneParamPresent = false;
    bool m_FsFbpaParamPresent = false;
    bool m_FsSpareParamPresent = false;

    // PriRing init is needed for any GPC & FBP floorsweeping as pri_ring hooks up
    // SYS,FBP&GPC clusters. Consequently, any FS change requiring a pri_ring init
    // would also require a graphics reset.
    bool m_IsPriRingInitRequired = false;

private:
    void        Init();
    static bool PollMasterRingCommand(void *pvArgs);
};

#endif

