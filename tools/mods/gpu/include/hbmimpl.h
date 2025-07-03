/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_HBMIMPL_H
#define INCLUDED_HBMIMPL_H

#include "gpu/utility/hbmdev.h"
#include "mods_reg_hal.h"

class GpuSubdevice;

//------------------------------------------------------------------------------
// This is the primary implementation class for performing tasks related to HBM
class HBMImpl
{

public:
    HBMImpl(GpuSubdevice* pSubdev) : m_HBMResetDelayMs(0), m_pGpuSub(pSubdev) { }
    virtual ~HBMImpl() { }

    HBMDevicePtr GetHBMDev() const { return m_pHBMDev; }
    void SetHBMDev(HBMDevicePtr pHBMDev) { m_pHBMDev = pHBMDev; }
    void SetHBMResetDelayMs(UINT32 timeMs) { m_HBMResetDelayMs = timeMs; }
    virtual RC ShutDown() { return OK; }

    virtual RC AssertFBStop() = 0;
    virtual RC DeAssertFBStop() = 0;

    virtual RC ReadHostToJtagHBMReg
    (
        const UINT32 siteID
        ,const UINT32 instructionID
        ,vector<UINT32>* dataRead
        ,const UINT32 dataReadLength
    ) const = 0;

    virtual RC WriteHostToJtagHBMReg
    (
        const UINT32 siteID
        ,const UINT32 instructionID
        ,const vector<UINT32>& writeData
        ,const UINT32 dataWriteLength
    ) const = 0;

    virtual RC SelectChannel(const UINT32 siteID, const UINT32 channel) const = 0;

    virtual RC InitSiteInfo
    (
        const UINT32 siteID
        ,const bool useHostToJtag
        ,HbmDeviceIdData* pDevIdData
    ) const;

    virtual FLOAT64 GetPollTimeoutMs() const;
    virtual RC WaitForTestI1500(UINT32 site, bool busyWait, FLOAT64 timeoutMs = -1.0f) const;

    virtual RC AssertMask(const UINT32 siteID) const = 0;

    virtual RC ResetSite(const UINT32 siteID) const = 0;

    virtual RC ToggleClock(const UINT32 siteID, const UINT32 numTimes) const = 0;

    virtual RC WriteHBMDataReg
    (
        const UINT32 siteID
        ,const UINT32 instruction
        ,const vector<UINT32>& writeData
        ,const UINT32 writeDataSize
        ,const bool useHostToJtag
    ) const = 0;

    virtual RC ReadHBMLaneRepairReg
    (
        const UINT32 siteID
        ,const UINT32 instruction
        ,const UINT32 channel
        ,vector<UINT32>* readData
        ,const UINT32 readSize
        ,const bool useHostToJtag
    ) const = 0;

    virtual RC GetDwordAndByteOverFbpaHbmBoundary
    (
        const UINT32 siteID
        ,const UINT32 laneBit
        ,UINT32* pDword
        ,UINT32* pByte
    ) const;

    virtual RC WriteBypassInstruction
    (
        const UINT32 siteID
        ,const UINT32 channel
    ) const = 0;

    RC DumpRepairedLanes
    (
        const UINT32 hwFbpa
        ,const UINT32 subp
        ,const UINT32 dword
        ,UINT32* pNumRepairs
    ) const;

    RC GetLaneRepairsFromLinkRepairReg
    (
        UINT32 hwFbpa
        ,UINT32 subp
        ,UINT32 fbpaDword
        ,vector<LaneError>* const pOutLaneRepairs
    ) const;

    RC GetLaneRepairsFromHbmFuse
    (
        UINT32 hbmSite
        ,UINT32 hbmChannel
        ,UINT32 hbmDword
        ,UINT32 remapValue
        ,vector<LaneError>* const pOutLaneRepairs
    ) const;

    RC ExtractRemapValueData
    (
        UINT32 remapValue
        ,vector<UINT32>* const pOutDwordLaneOffsets
        ,vector<LaneError::RepairType>* const pOutRepairTypes
    ) const;

    virtual RC GetVoltageUv(UINT32 *pVoltUv) { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC SetVoltageUv(UINT32 voltUv) { return RC::UNSUPPORTED_FUNCTION; }

protected:
    virtual RC GetHBMDevIdDataHtoJ
    (
        const UINT32 siteID
        ,vector<UINT32>* pRegValues
    ) const = 0;
    virtual RC GetHBMDevIdDataFbPriv
    (
        const UINT32 siteID
        ,vector<UINT32>* pRegValues
    ) const = 0;

    UINT32        m_HBMResetDelayMs;
    HBMDevicePtr  m_pHBMDev;       //!< Pointer to HBM device
    GpuSubdevice* m_pGpuSub;       //!< GpuSubdevice associated with this HBM instance
};

// Macro to create a function for creating a concrete HBM interface
#define CREATE_HBM_FUNCTION(Class)             \
    HBMImpl *Create##Class(GpuSubdevice *pGpu) \
    {                                          \
        return new Class(pGpu);                \
    }

#endif
