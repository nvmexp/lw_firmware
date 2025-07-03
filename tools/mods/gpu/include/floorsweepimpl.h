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
 * This is the class interface for the Floorsweep implementor class on Kepler+
 * style of floorsweeping.
 * Floorsweeping is typically done during bringup, on fmodel, and during lots
 * of arch tests.
 * Floorsweeping is not done on production test systems without good cause.
 * To floorsweep something there would be a command line argument as follows:
 * -fermi_fs <feature-string> where
 * feature-string = <name>:<value>[:<value>...]
 */
#ifndef INCLUDED_FLOORSWEEPIMPL_H
#define INCLUDED_FLOORSWEEPIMPL_H

#include "core/include/argpproc.h"
#include "core/include/argread.h"
#include "core/include/argdb.h"
#include "core/include/fileholder.h"
#include "core/include/jsonlog.h"

#include <map>

class GpuSubdevice;

class FloorsweepImpl
{
public:
    typedef map<string, UINT32> FloorsweepingMasks;

    // Constructor
    FloorsweepImpl( GpuSubdevice *pSubdev);
    virtual ~FloorsweepImpl() {}

    // Apply all floorsweeping changes passed in from the command line
    virtual bool    IsFloorsweepingAllowed() const;
    virtual RC      ApplyFloorsweepingChanges(bool InPreVBIOSSetup);
    virtual bool    GetFloorsweepingAffected() const;
    virtual RC      GetFloorsweepingMasks(FloorsweepingMasks* pFsMasks) const;
    virtual RC      PreserveFloorsweepingSettings() const;
    virtual RC      PrintPreservedFloorsweeping() const;
    virtual void    PostRmShutdown(bool bInPreVBIOSSetup) {}
    virtual void    SetVbiosControlsFloorsweep(bool bControlsFs) {};

    // Reset any internal floorsweeping state.
    virtual void    Reset();

    // Hopper+ masks
    virtual bool    SupportsCpcMask(UINT32 hwGpcNum) const;
    virtual UINT32  CpcMask(UINT32 hwGpcNum) const;
    virtual UINT32  LwLinkLinkMask() const;
    virtual UINT32  LwLinkLinkMaskDisable() const;
    virtual UINT32  C2CMask() const;
    virtual UINT32  C2CMaskDisable() const;

    // GA10x+ mask
    virtual bool    SupportsRopMask(UINT32 hwGpcNum) const;
    virtual UINT32  RopMask(UINT32 hwGpcNum) const;
    virtual bool    SupportsL2SliceMask(UINT32 hwFbpNum) const;
    virtual UINT32  L2SliceMask(UINT32 HwFbpNum) const;

    // Ampere + masks
    virtual UINT32  SyspipeMask() const;
    virtual UINT32  SyspipeMaskDisable() const;
    virtual UINT32  OfaMask() const;
    virtual UINT32  OfaMaskDisable() const;
    virtual UINT32  LwjpgMask() const;
    virtual UINT32  LwjpgMaskDisable() const;

    // Turing + masks
    virtual UINT32  LwLinkMask() const;

    // Volta + masks
    virtual UINT32  PceMask() const;

    // Gpu specific APIs must be implemented by derived class.
    // Pascal+ masks
    virtual UINT32  PesOnGpcStride() const;
    virtual UINT32  PesMask(UINT32 HwGpcNum) const;
    virtual UINT32  GetHalfFbpaWidth() const;
    virtual UINT32  HalfFbpaMask() const;

    // Maxwell* masks
    virtual UINT32 LwencMask() const;
    virtual UINT32 LwencMaskDisable() const;
    virtual UINT32 LwdecMask() const;
    virtual UINT32 LwdecMaskDisable() const;

    // Fermi+ masks
    UINT32  FbioMask() const { return FbioMaskImpl(); }
    UINT32  FbioshiftMask() const { return FbioshiftMaskImpl(); }
    UINT32  FbMask() const { return FbMaskImpl(); }
    UINT32  FbpMask() const { return FbpMaskImpl(); }
    UINT32  L2Mask(UINT32 hwFbpNum) const { return L2MaskImpl(hwFbpNum); }
    UINT32  L2Mask() const { return L2MaskImpl(); }
    UINT32  FbpaMask() const { return FbpaMaskImpl(); }

    virtual UINT32  FscontrolMask() const;
    virtual UINT32  GpcMask() const;
    virtual UINT32  GfxGpcMask() const;
    virtual UINT32  TpcMask(UINT32 HwGpcNum) const;
    virtual UINT32  TpcMask() const;
    virtual UINT32  SpareMask() const;
    virtual UINT32  ZlwllMask(UINT32 HwGpcNum) const;

    // Tesla & Fermi+ masks
    virtual UINT32  GetFbEnableMask() const;

    // Tesla masks
    virtual UINT32  GetTpcEnableMask() const;
    virtual UINT32  GetMevpEnableMask() const;
    virtual UINT32  GetSmEnableMask() const;
    virtual UINT32  MevpMask() const;
    virtual UINT32  SmMask(UINT32 HwGpcNum, UINT32 HwTpcNum) const;
    virtual UINT32  SmMask() const { return SmMask(~0U, ~0U); }
    virtual UINT32  XpMask() const;
    virtual RC      PrintMiscFuseRegs();
    virtual RC      SwReset(bool bReinitPriRing);
    RC              GenerateSweepJson() const;

protected:
    virtual void            ApplyFloorsweepingSettings();
    virtual RC              CheckFloorsweepingArguments();
    virtual string          GetFermiFsArgs();
    virtual bool            GetFloorsweepingChangesApplied() const;
    virtual Tee::Priority   GetFloorsweepingPrintPriority() const;
    virtual UINT32          GetGpcEnableMask() const;
    virtual void            GetTpcEnableMaskOnGpc(vector<UINT32> *pValues) const;
    virtual void            GetZlwllEnableMaskOnGpc(vector<UINT32> *pValues) const;
    virtual bool            GetRestoreFloorsweeping() const;
    virtual bool            GetResetFloorsweeping() const;
    virtual bool            GetAdjustFloorsweepingArgs() const;
    virtual bool            GetAddLwrFsMask() const;
    virtual bool            GetAdjustL2NocFloorsweepingArgs() const;
    virtual RC              PrepareArgDatabase(string FsArgs, ArgDatabase* fsArgDatabase);
    virtual void            PrintFloorsweepingParams();
    virtual RC              ReadFloorsweepingArguments();
    virtual RC              AdjustDependentFloorsweepingArguments();
    virtual RC              AdjustGpcFloorsweepingArguments();
    virtual RC              AdjustFbpFloorsweepingArguments();
    virtual RC              AdjustL2NocFloorsweepingArguments();
    virtual RC              AddLwrFsMask();
    virtual void            RestoreFloorsweepingSettings();
    virtual void            ResetFloorsweepingSettings();
    virtual void            SaveFloorsweepingSettings();
    virtual void            SetFloorsweepingAffected(bool affected);
    virtual void            SetFloorsweepingChangesApplied(bool bApplied);
    bool                    IsFbBroken() const;
    virtual bool            IsCeFloorsweepingSupported() const { return true; }
    virtual bool            IsFloorsweepingValid() const { return true; }
    virtual void            PopulateGpcDisableMasks(JsonItem *pJsi) const {}
    virtual void            PopulateFbpDisableMasks(JsonItem *pJsi) const {}
    virtual void            PopulateMiscDisableMasks(JsonItem *pJsi) const {}

    // Fix any floorweeping inconsistencies post ATE level floorsweeping
    virtual RC              FixFloorsweepingInitMasks(bool *pFsAffected);
    virtual bool            GetFixFloorsweepingInitMasks();

    GpuSubdevice * m_pSub;

private:
    bool m_FloorsweepingAffected;
    virtual UINT32  FbioMaskImpl() const;
    virtual UINT32  FbioshiftMaskImpl() const;
    virtual UINT32  FbMaskImpl() const;
    virtual UINT32  FbpMaskImpl() const;
    virtual UINT32  L2MaskImpl(UINT32 HwFbpNum) const;
    virtual UINT32  L2MaskImpl() const;
    virtual UINT32  FbpaMaskImpl() const;
    bool            GenerateSweepJsonArgPresent() const;

    // If true floor sweeping changes have been applied
    bool m_AppliedFloorsweepingChanges;

};

#define CREATE_GPU_FS_FUNCTION(Class)                   \
    FloorsweepImpl * Create##Class(GpuSubdevice *pGpu)  \
    {                                                   \
        return new Class(pGpu);                         \
    }

#define FS_ADD_NAMED_PROPERTY(Name, InitType, Default)       \
    {                                                        \
        InitType Value = Default;                            \
        GpuSubdevice::NamedProperty initProp(Value);         \
        m_pSub->AddNamedProperty(#Name, initProp);           \
    }

#define FS_ADD_ARRAY_NAMED_PROPERTY(Name, Size, Default)        \
    {                                                           \
        GpuSubdevice::NamedProperty initProp(Size, Default);    \
        m_pSub->AddNamedProperty(#Name, initProp);              \
    }

#endif

