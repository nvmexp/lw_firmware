/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/include/hbmimpl.h"
#include "core/include/memerror.h"

#include <vector>

class GpuSubdevice;

class MBistImpl
{
public:

    // Types of MBIST sequences
    enum
    {
        HBM_MBIST_TYPE_SCAN_PATTERN  ,
        HBM_MBIST_TYPE_MARCH_PATTERN ,
        HBM_MBIST_TYPE_HIGH_FREQUENCY,
        HBM_MAX_MBIST_TYPES          ,
        HBM_MBIST_UNKNOWN_TYPE
    };

    MBistImpl() : m_pHbmDev(nullptr), m_UseHostToJtag(false), m_StackHeight(0) { }

    MBistImpl(HBMDevice* pHbmDev, bool htoj, UINT32 stackHeight)
    : m_pHbmDev(pHbmDev), m_UseHostToJtag(htoj), m_StackHeight(stackHeight) { }

    virtual ~MBistImpl(){}

    HBMDevice* GetHbmDevice() const { return m_pHbmDev; }
    GpuSubdevice *GetGpuSubdevice() const { return m_pHbmDev->GetGpuSubdevice(); }
    HBMImpl *GetHBMImpl() const { return GetGpuSubdevice()->GetHBMImpl(); }
    bool IsHostToJtagPath() const { return m_UseHostToJtag; }
    UINT32 GetStackHeight() const { return m_StackHeight; }

    RC GetRepairedLanes
    (
        const UINT32 siteID
        ,const UINT32 hbmChannel
        ,vector<LaneError>* const pOutLaneRepairs
    ) const;
    RC LaneRepair
    (
        const bool isSoft
        ,const bool skipVerif
        ,const bool isPseudoRepair
        ,const UINT32 siteID
        ,const UINT32 hbmChannel
        ,const LaneError& laneError
    ) const;

    // Vendor specific implementations
    virtual RC StartMBist(const UINT32 siteID, const UINT32 mbistType) = 0;

    virtual RC CheckCompletionStatus(const UINT32 siteID, const UINT32 mbistType) = 0;

    virtual RC SoftRowRepair
    (
        const UINT32 siteID
        ,const UINT32 stackID
        ,const UINT32 channel
        ,const UINT32 row
        ,const UINT32 bank
        ,const bool firstRow
        ,const bool lastRow
    ) = 0;

    virtual RC HardRowRepair
    (
        const bool isPseudoRepair
        ,const UINT32 siteID
        ,const UINT32 stackID
        ,const UINT32 channel
        ,const UINT32 row
        ,const UINT32 bank
    ) const = 0;

    virtual RC DidJtagResetOclwrAfterSWRepair(bool *pReset) const = 0;

    virtual RC GetNumSpareRowsAvailable
    (
        const UINT32 siteID
        ,const UINT32 stackID
        ,const UINT32 channel
        ,const UINT32 bank
        ,UINT32 *pNumAvailable
        ,bool doPrintResults = true
    ) const = 0;

private:
    HBMDevice* m_pHbmDev;
    bool m_UseHostToJtag;
    UINT32 m_StackHeight;

    UINT16 GetLaneRepairData(const vector<UINT32>& ieeeData, UINT32 dword) const;
    UINT16 ModifyLaneRepairData
    (
        vector<UINT32>* const pIeeeData
        ,UINT32 dword
        ,UINT16 remapVal
        ,UINT16 mask
    ) const;

    RC SoftLaneRepair
    (
        vector<UINT32>* const pIeeeData
        ,const UINT32 siteID
        ,const UINT32 hbmChannel
        ,const UINT32 hbmDword
        ,const UINT16 remapVal
        ,const UINT16 remapValMask
    ) const;
    RC HardLaneRepair
    (
        vector<UINT32>* const pIeeeData
        ,const bool isPseudoRepair
        ,const UINT32 siteID
        ,const UINT32 hbmChannel
        ,const UINT32 hbmDword
        ,const UINT16 remapVal
        ,const UINT16 remapValMask
    ) const;
};
