/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "ga10xfs.h"

class ArgReader;

class HopperFs : public GA10xFs
{
public:
    explicit HopperFs(GpuSubdevice *pSubdev);
    virtual ~HopperFs() {}

protected:
    void    SetVbiosControlsFloorsweep(bool bControlsFs) override;
    RC      ApplyFloorsweepingChanges(bool bInPreVBIOSSetup) override;
    void    PostRmShutdown(bool bInPreVBIOSSetup) override;
    RC      InitializePriRing_FirstStage(bool bInSetup) override;
    RC      InitializePriRing() override;
    void    ApplyFloorsweepingSettings() override;
    RC      GetFloorsweepingMasks(FloorsweepingMasks* pFsMasks) const override;
    void    PrintFloorsweepingParams() override;
    RC      ReadFloorsweepingArgumentsImpl(ArgDatabase& fsArgDb) override;
    void    RestoreFloorsweepingSettings() override;
    void    ResetFloorsweepingSettings() override;
    void    SaveFloorsweepingSettings() override;
    bool    IsFloorsweepingValid() const override;
    RC      PrintMiscFuseRegs() override;
    void    CheckIfPriRingInitIsRequired() override;
    RC      PopulateFsInforom(LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS *pIfrParams) const override;

    RC      AdjustDependentFloorsweepingArguments() override;
    RC      AdjustPreGpcFloorsweepingArguments(UINT32 *pTpcMaskModified) override;
    RC      AdjustPesFloorsweepingArguments(UINT32 *pTpcMaskModified) override;
    virtual RC AdjustCpcFloorsweepingArguments(UINT32 *pTpcMaskModified);
    RC      AdjustL2NocFloorsweepingArguments() override;
    RC FixFbpFloorsweepingMasks
    (
        UINT32 fbNum,
        bool *pFbMaskModified,
        bool *pFbioMaskModified,
        UINT32 *pL2MaskModified,
        UINT32 *pL2SliceMaskModified,
        bool *pFsAffected
    ) override;
    RC FixFbpLtcFloorsweepingMasks
    (
        UINT32 fbNum,
        UINT32 *pL2MaskModified,
        UINT32 *pL2SliceMaskModified,
        bool *pFsAffected
    ) override;
    RC FixHalfFbpaFloorsweepingMasks
    (
        bool *pFsAffected
    ) override;

    bool    SupportsCpcMask(UINT32 hwGpcNum) const override;
    UINT32  CpcMask(UINT32 hwGpcNum) const override;
    UINT32  LwLinkLinkMask() const override;
    UINT32  LwLinkLinkMaskDisable() const override;
    UINT32  C2CMask() const override;
    UINT32  C2CMaskDisable() const override;
    void    PopulateGpcDisableMasks(JsonItem *pJsi) const override;
    void    PopulateMiscDisableMasks(JsonItem *pJsi) const override;

#ifdef INCLUDE_FSLIB
    void GetFsLibMask(FsLib::GpcMasks *pGpcMasks) const override;
#endif

    RC      SwReset(bool bReinitPriRing) override;

    vector<bool>   m_FsGpcCpcParamPresent;
    vector<UINT32> m_GpcCpcMasks;

private:
    vector<UINT32> m_SavedGpcCpcMasks;
    vector<FsData> m_FsData;

    void    AdjustLwLinkPerlinkArgs();
    void    AssertFsSampleSignal();
    UINT32  GetMaxCpcCount() const;
    static bool PollPriRingCmdNone(void *pRegs);
    static bool PollPriRingStatus(void *pRegs);
};

