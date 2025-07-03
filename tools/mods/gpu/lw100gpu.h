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

#ifndef INCLUDED_GV100GPU_H
#define INCLUDED_GV100GPU_H

#include "voltagpu.h"
#include "gpu/include/device_interfaces.h"

class GV100GpuSubdevice final : public VoltaGpuSubdevice
                              , public DeviceInterfaces<Device::GV100>::interfaces
{
    using HbmSite = Memory::Hbm::Site;

public:
    GV100GpuSubdevice(const char*, Gpu::LwDeviceId, const PciDevInfo*);
    void SetupBugs() override;
    bool GetHBMTypeInfo(string *pVendor, string *pRevision) const override;

    RC DramclkDevInitWARkHz(UINT32 freqkHz) override;
    RC SetClock
    (
        Gpu::ClkDomain clkDomain,
        UINT64 targetHz,
        UINT32 PStateNum,
        UINT32 flags
    ) override;
    RC GetClock
    (
        Gpu::ClkDomain which,
        UINT64 * pActualPllHz,
        UINT64 * pTargetPllHz,
        UINT64 * pSlowedHz,
        Gpu::ClkSrc * pSrc
    ) override;

    RC GetHBMSiteMasterFbpaNumber(const UINT32 siteID,
                                  UINT32* const pFbpaNum) const override;

    RC GetHBMSiteFbps
    (
        const UINT32 siteID,
        UINT32* const pFbpNum1,
        UINT32* const pFbpNum2
    ) const override;

    RC GetHbmLinkRepairRegister
    (
        const LaneError& laneError
        ,UINT32* pOutRegAddr
    ) const override;

    RC GetMaxL1CacheSizePerSM(UINT32* pSizeBytes) const override
    {
        MASSERT(pSizeBytes);
        *pSizeBytes = static_cast<UINT32>(128_KB);
        return RC::OK;
    }
    double GetArchEfficiency(Instr type) override;

    RC GetAndClearL2EccCounts
    (
        UINT32 ltc,
        UINT32 slice,
        L2EccCounts * pL2EccCounts
    ) override;
    RC SetL2EccCounts
    (
        UINT32 ltc,
        UINT32 slice,
        const L2EccCounts & l2EccCounts
    ) override;
    RC GetAndClearL2EccInterrupts
    (
        UINT32 ltc,
        UINT32 slice,
        bool *pbCorrInterrupt,
        bool *pbUncorrInterrupt
    ) override;

    UINT32 GetNumHbmSites() const override;

    RC CheckHbmIeee1500RegAccess(const HbmSite& hbmSite) const override;

private:
    TestDevice* GetDevice() override { return this; }
    const TestDevice* GetDevice() const override { return this; }
    static RC CallwlateDivsForTargetFreq
    (
        UINT32 targetkHz,
        UINT32* pPDiv,
        UINT32* pNDiv,
        UINT32* pMDiv
    );
    RC GetFirstValidHBMPllNumber(UINT32 *pOutHbmPllNum);
};

#endif // INCLUDED_GV100GPU_H
