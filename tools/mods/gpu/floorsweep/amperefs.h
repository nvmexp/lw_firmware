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

#include "tu10xfs.h"

#ifdef INCLUDE_FSLIB
    #include "fslib_interface.h"
#endif

class ArgReader;

class AmpereFs : public TU10xFs
{
public:
    explicit AmpereFs(GpuSubdevice *pSubdev);
    virtual ~AmpereFs() {}

protected:
    RC      InitializePriRing_FirstStage(bool bInSetup) override;
    void    ApplyFloorsweepingSettings() override;
    RC      GetFloorsweepingMasks(FloorsweepingMasks* pFsMasks) const override;
    void    PrintFloorsweepingParams() override;
    RC      ReadFloorsweepingArgumentsImpl(ArgDatabase& fsArgDb) override;
    void    RestoreFbio() const override;
    void    ResetFbio() const override;
    void    RestoreFloorsweepingSettings() override;
    void    ResetFloorsweepingSettings() override;
    void    SaveFloorsweepingSettings() override;
    bool    IsFloorsweepingValid() const override;
    RC      PrintMiscFuseRegs() override;
    UINT32  LwdecMask() const override;
    UINT32  LwencMask() const override;
    UINT32  SyspipeMask() const override;
    UINT32  OfaMask() const override;
    UINT32  LwjpgMask() const override;
    UINT32  PceMask() const override;
    UINT32  LwLinkMask() const override;
    void    CheckIfPriRingInitIsRequired() override;
    RC      AdjustL2NocFloorsweepingArguments() override;
    RC      AddLwrFsMask() override;
    RC      PreserveFloorsweepingSettings() const override;
    bool    IsIfrFsEntry(UINT32 address) const override;
    RC      PopulateFsInforom(LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS *pIfrParams) const override;
    void    PopulateGpcDisableMasks(JsonItem *pJsi) const override;
    void    PopulateFbpDisableMasks(JsonItem *pJsi) const override;
    void    PopulateMiscDisableMasks(JsonItem *pJsi) const override;

#ifdef INCLUDE_FSLIB
    virtual void GetFsLibMask(FsLib::GpcMasks *pGpcMasks) const;
    virtual void GetFsLibMask(FsLib::FbpMasks *pFbpMasks) const;
#endif

    enum FloorsweepType
    {
        FST_SYSPIPE
       ,FST_OFA
       ,FST_LWJPG
       ,FST_LWLINK_LINK
       ,FST_C2C
       ,FST_COUNT
    };
    struct FsData
    {
        FloorsweepType  type         = FST_COUNT;
        string          name;
        string          disableStr;
        ModsGpuRegField ctrlField    = MODS_REGISTER_FIELD_NULL;
        ModsGpuRegField disableField = MODS_REGISTER_FIELD_NULL;
        bool            bPresent     = false;
        UINT32          mask         = 0U;
        UINT32          savedMask    = 0U;
        FsData() { }
        FsData(FloorsweepType t, string n, string ds, ModsGpuRegField c, ModsGpuRegField d) :
            type(t), name(n), disableStr(ds), ctrlField(c), disableField(d) { }
    };

    RC ReadFloorsweepArg(ArgReader * pArgRead, FsData *pFsData);
    void ApplyFloorsweepingSettings(const vector<FsData> & fsData);
    void GetFloorsweepingStrings
    (
        const vector<FsData> & fsData,
        string * pParamsStr,
        string * pMaskStr
    );
    void RestoreFloorsweepingSettings(const vector<FsData> & fsData);
    void ResetFloorsweepingSettings(const vector<FsData> & fsData);
    void SaveFloorsweepingSettings(vector<FsData> * pFsData);
    void PrintMiscFuseRegs(const vector<FsData> & fsData);

    void GraphicsEnable(bool bEnable) override;

    RC  SwReset(bool bReinitPriRing) override;
    UINT32 MaskFromHwDevInfo(GpuSubdevice::HwDevType devType) const;

private:
    UINT32  GetEngineResetMask() override;

    vector<FsData> m_FsData;
};
