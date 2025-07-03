/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#pragma once

#include "core/include/devmgr.h"
#include "core/include/framebuf.h"
#include "core/include/hbm_model.h"
#include "core/include/memerror.h"
#include "core/include/memtypes.h"
#include "ctrl/ctrl90e7.h"
#include "gpu/interface/vbios_preos_pbi.h"
#include "gpu/hbm/hbm_spec_devid.h"
#include "gpu/include/gpusbdev.h"

#include <string>
#include <vector>

class MBistImpl;
class HBMImpl;

//-----------------------------------------------------------------------------
//! \brief HBM site implementation
//!
//! A HBM device is made up of a number of 4 HBM sites
class HBMSite
{
public:
    HBMSite();
    ~HBMSite();

    bool IsSiteActive() const { return m_IsActive; }
    void SetSiteActive(bool active) { m_IsActive = active; }

    // Set vendor specific MBIST implementation
    void SetMBistImpl(MBistImpl * mbist);

    RC StartMBistAtSite (const UINT32 siteID,
        const UINT32 mbistType);

    RC CheckMBistCompletionAtSite (const UINT32 siteID,
        const UINT32 mbistType);

    RC InitSiteInfo(GpuSubdevice *pSubdev, UINT32 siteId, bool useHostToJtag);
    RC InitSiteInfo(GpuSubdevice *pSubdev, UINT32 siteId, const vector<UINT32>& rawDevId);

    RC DeInitSite();
    const HbmDeviceIdData GetSiteInfo() const;

    RC DidJtagResetOclwrAfterSWRepair(bool *pReset) const;

    RC RepairAtSite
    (
        const bool isSoftRepair
        ,const bool isPseudoRepair
        ,const bool skipVerif
        ,const UINT32 stackID
        ,const UINT32 channel
        ,const UINT32 row
        ,const UINT32 bank
        ,const bool firstRow
        ,const bool lastRow
    ) const;

    RC RepairLaneAtSite
    (
        const bool isSoftRepair
        ,const bool skipVerif
        ,const bool isPseudoRepair
        ,const UINT32 hbmChannel
        ,const LaneError& laneError
    ) const;

    RC DumpDevIdData() const;

    RC DumpRepairedRows() const;

    RC GetRepairedLanes(vector<LaneError>* pOutLaneRepairs) const;

private:

    struct BankRepair
    {
        UINT32 m_BankID;
        UINT32 m_NumRepairs;
    };

    bool m_Initialized;
    bool m_UseHostToJtag;
    bool m_IsActive;        //!< If this site is active
    bool m_MBistStarted;    //!< If MBIST has been started on this site
    UINT32 m_SiteID;

    HbmDeviceIdData m_DevIdData;

    // Pointer to vendor specific MBIST implementation
    shared_ptr<MBistImpl> m_MbistImpl;
    GpuSubdevice *m_pSubdev;
};

//-----------------------------------------------------------------------------
//! \brief HBM device implementation
//!
class HBMDevice : public Device
{
    using HbmManufacturerId = HbmDeviceIdData::Manufacturer;

public:
    //! Maps row name of a failing row to the test ID that discovered it.
    using RowErrorOriginTestIdMap = map<string, INT32>;

    HBMDevice(GpuSubdevice *);
    virtual ~HBMDevice(){}

    virtual RC Initialize();
    RC Initialize(const vector<Memory::DramDevId>& hbmSiteDevIds);

    virtual RC Shutdown();

    virtual RC AssertFBStop() const;
    virtual RC DeAssertFBStop() const;

    // MBist specific functionality
    virtual RC StartMBist(const UINT32 mbistType);
    virtual RC CheckMBistCompletion(const UINT32 mbistType);

    virtual RC RowRepair
    (
        const bool isSoftRepair
        ,const vector<Memory::Row>& repairRows
        ,const RowErrorOriginTestIdMap& originTestMap
        ,const bool skipVerif
        ,const bool isPseudoRepair
    );
    virtual RC LaneRepair
    (
        const bool isSoftRepair
        ,const vector<LaneError>& repairLanes
        ,bool skipVerif
        ,bool isPseudoRepair
    );

    virtual RC DidJtagResetOclwrAfterSWRepair(bool *pReset) const;

    // Get features
    GpuSubdevice *GetGpuSubdevice() const   { return m_pSubdev; }
    bool IsECCSupported() const             { return m_ECCSupported;}
    UINT32 GetStackHeight() const           { return m_StackHeight;}
    UINT32 GetNumSites() const              { return m_NumActiveSites; }
    vector<UINT64> GetEcids() const         { return m_Ecids;}
    HBMImpl * GetHBMImpl() const            { return m_pSubdev->GetHBMImpl(); }

    void ForceHostToJtag() { m_UseHostToJtag = true; }

    RC DumpDevIdData() const;
    RC DumpRepairedRows() const;
    RC DumpRepairedLanes(bool readHBMData) const;

    RC PopulateInfoRomSoftRepairObj() const;
    RC ClearInfoRomSoftSoftRepairObj() const;
    RC ValidateInfoRomSoftRepairObj() const;
    RC PrintInfoRomSoftRepairObj() const;

    static UINT32 RemapValueBytePairIndex(UINT32 remapValue);
    static RC CalcLaneMaskAndRemapValue
    (
        LaneError::RepairType repairType
        ,UINT32 laneBit
        ,UINT16* const pOutMask
        ,UINT16* const pOutRemapValue
    );
    static RC GetLaneRemappingValues
    (
        Gpu::LwDeviceId deviceId
        ,const vector<LaneError>& laneErrors
    );

    RC LaneRepairsFromHbmFuse
    (
        UINT32 hbmSite
        ,UINT32 hbmChannel
        ,UINT32 hbmDword
        ,UINT32 remapValue
        ,vector<LaneError>* pOutLaneErrors
    ) const;

    RC GetHbmSiteChannel
    (
        const LaneError& laneError
        ,UINT32* pOutSite
        ,UINT32* pOutChannel
    ) const;
    UINT32 GetRemapValue(const LaneError& laneError) const;
    UINT32 GetSubpartition(const LaneError& laneError) const;

    RC GetHbmModel(Memory::Hbm::Model* hbmModel) const;
    const vector<HBMSite>& GetHbmSites() { return m_Sites; }

private:
    GpuSubdevice *m_pSubdev;

    bool m_UseHostToJtag;
    bool m_ECCSupported;
    bool m_Initialized;

    UINT32 m_StackHeight;
    HbmManufacturerId m_VendorID;
    vector<UINT64> m_Ecids;
    UINT32 m_NumActiveSites;

    // vector of sites in the HBM device
    vector<HBMSite> m_Sites;

    INT32 m_LastSiteSWRepaired;     //used for checking for JTAG reset

    RC GetRepairedGpuLanes(vector<LaneError>* const pOutLaneRepairs) const;
    RC GetRepairedHbmLanes(vector<LaneError>* const pOutLaneRepairs) const;

    RC ReadInfoRom(LW90E7_CTRL_RPR_GET_INFO_PARAMS* infoRomParams) const;
    RC WriteInfoRom(LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS* infoRomParams) const;

    RC CollectInfoRomLaneRepairData
    (
        LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS* pOutIfrParams
    ) const;

    bool IsAllSameHbmModel(const vector<HBMSite>& sites) const;

    RC CollectSiteInfoFromHbm();
    RC PostProcessSiteInfo(HBMSite* pSite, UINT32 siteId, bool* pSuccessfullyCollected);
};

// Use smart pointer
typedef shared_ptr<HBMDevice> HBMDevicePtr;

//!
//! \brief Collection of HBM site device IDs associated with a particular GPU
//! subdevice.
//!
struct GpuDramDevIds
{
    GpuSubdevice* pSubdev = nullptr; //!< GPU subdevice.
    bool hasData = false;            //!< Is data available to read.
    vector<Memory::DramDevId> dramDevIds; //!< DRAM chip device IDs.

    GpuDramDevIds(GpuSubdevice* pSubdev) : pSubdev(pSubdev) {}
};

//-----------------------------------------------------------------------------
//! \brief Manage the HBM devices
//!
class HBMDevMgr : public DevMgr
{
public:
    HBMDevMgr();
    ~HBMDevMgr(){}

    RC AllocateDevice();
    RC ShutdownAll();

    void SetInitMethod(Gpu::HbmDeviceInitMethod method) { m_InitMethod = method; }

    virtual RC       InitializeAll();
    RC               InitializeAll(const vector<GpuDramDevIds>& allGpusDramDevIds);

    virtual UINT32   NumDevices();
    virtual RC       FindDevices();
    virtual void     FreeDevices(){}
    virtual RC       GetDevice(UINT32 index, Device **device);
private:
    bool                     m_Initialized;
    Gpu::HbmDeviceInitMethod m_InitMethod;

    //vector of HBM devices
    vector<HBMDevicePtr> m_Devices;
};

//-----------------------------------------------------------------------------
//! \brief JS Interface to HBM device
//!
class JsHBMDev
{
public:
    JsHBMDev();

    RC           CreateJSObject(JSContext *cx, JSObject *obj);
    JSObject *   GetJSObject() {return m_pJsHBMDevObj;}
    HBMDevicePtr GetHBMDev();
    void         SetHBMDev(HBMDevicePtr pHBMDev);

private:
    HBMDevicePtr m_pHBMDev;
    JSObject   * m_pJsHBMDevObj;
};
