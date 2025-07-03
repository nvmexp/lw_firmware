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

#include "amperefs.h"
#include "ctrl/ctrl90e7.h"

class ArgReader;

class GA10xFs : public AmpereFs
{
public:
    explicit GA10xFs(GpuSubdevice *pSubdev);
    virtual ~GA10xFs() {}

protected:
    void    ApplyFloorsweepingSettings() override;
    RC      GetFloorsweepingMasks(FloorsweepingMasks* pFsMasks) const override;
    void    PrintFloorsweepingParams() override;
    RC      ReadFloorsweepingArgumentsImpl(ArgDatabase& fsArgDb) override;
    void    RestoreFloorsweepingSettings() override;
    void    ResetFloorsweepingSettings() override;
    void    SaveFloorsweepingSettings() override;
    void    CheckIfPriRingInitIsRequired() override;
    RC      PrintMiscFuseRegs() override;
    RC      FixFloorsweepingInitMasks(bool *pFsAffected) override;
    RC      PopulateFsInforom(LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS *pIfrParams) const override;
    RC      AddLwrFsMask() override;
    void    PopulateGpcDisableMasks(JsonItem *pJsi) const override;
    void    PopulateFbpDisableMasks(JsonItem *pJsi) const override;

    RC      AdjustL2NocFloorsweepingArguments() override;
    RC      AdjustPostGpcFloorsweepingArguments() override;
    RC      AdjustFbpFloorsweepingArguments() override;
    virtual RC FixFbpFloorsweepingMasks
    (
        UINT32 fbNum,
        bool *pFbMaskModified,
        bool *pFbioMaskModified,
        UINT32 *pL2MaskModified,
        UINT32 *pL2SliceMaskModified,
        bool *pFsAffected
    );
    virtual RC FixFbpLtcFloorsweepingMasks
    (
        UINT32 fbNum,
        UINT32 *pL2MaskModified,
        UINT32 *pL2SliceMaskModified,
        bool *pFsAffected
    );
    virtual RC FixHalfFbpaFloorsweepingMasks
    (
        bool *pFsAffected
    );

#ifdef INCLUDE_FSLIB
    void GetFsLibMask(FsLib::GpcMasks *pGpcMasks) const override;
    void GetFsLibMask(FsLib::FbpMasks *pFbpMasks) const override;
#endif

    bool    SupportsRopMask(UINT32 hwGpcNum) const override;
    UINT32  RopMask(UINT32 hwGpcNum) const override;
    bool    SupportsL2SliceMask(UINT32 hwFbpNum) const override;
    UINT32  L2SliceMask(UINT32 hwFbpNum) const override;
    UINT32  PceMask() const override;

    UINT32  GetMaxRopCount() const;
    UINT32  GetMaxSlicePerLtcCount() const;

    vector<bool>   m_FsGpcRopParamPresent;
    vector<UINT32> m_GpcRopMasks;

    vector<bool>   m_FsFbpL2SliceParamPresent;
    vector<UINT32> m_FbpL2SliceMasks;

private:
    vector<UINT32> m_SavedGpcRopMasks;
    vector<UINT32> m_SavedFbpL2SliceMasks;

    RC FixFbFloorsweepingInitMasks(bool *pFsAffected);
};

