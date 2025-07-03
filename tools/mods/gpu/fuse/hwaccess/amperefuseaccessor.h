/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpufuseaccessor.h"

class AmpereFuseAccessor : public GpuFuseAccessor
{
public:
    AmpereFuseAccessor(TestDevice* pSubdev);
    virtual ~AmpereFuseAccessor() = default;

    void SetParent(Fuse* pFuse) override;

    RC SetupFuseBlow() override;
    RC CleanupFuseBlow() override;

    RC BlowFuses(const vector<UINT32>& fuseBinary) override;
    RC SetSwIffFuses(const vector<UINT32>& iffRows, UINT32 iffCrc) override;
    RC VerifySwIffFusing(const vector<UINT32>& iffRows, UINT32 iffCrc) override
        { return RC::OK; }

    bool IsSwFusingAllowed() override;

    RC LatchFusesToOptRegs() override;

    // On Ampere the clock is 50MHz
    UINT32 GetFuseClockPeriodNs() const override { return 20; }

protected:
    // Clear state for HW fusing
    virtual RC SetupHwFusing();

private:
    // GPIO pin to toggle fuse enable
    static const UINT32 FUSE_EN_GPIO_NUM = 19; 
    
    // Pointer to test device to perform register operations
    TestDevice* m_pDev;

    // Pointer back to the main fuse object so it can perform
    // common fuse actions (e.g. get the value of disable_sw_override)
    Fuse* m_pFuse;

    // Get GPIO pin connected to FUSE_EN
    UINT32 GetFuseEnableGpioNum();

    // Poll until IFF fusing status is idle
    RC PollSwIffIdle();
};
