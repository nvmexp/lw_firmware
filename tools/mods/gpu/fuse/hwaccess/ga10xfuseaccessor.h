/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "amperefuseaccessor.h"

class GA10xFuseAccessor : public AmpereFuseAccessor
{
public:
    GA10xFuseAccessor(TestDevice* pSubdev);
    virtual ~GA10xFuseAccessor() = default;

    void SetParent(Fuse* pFuse) override;

    RC SetupFuseBlow() override;
    RC CleanupFuseBlow() override;

    RC BlowFuses(const vector<UINT32>& fuseBinary) override;

    RC LatchFusesToOptRegs() override;

    // On Ampere the clock is 27MHz
    UINT32 GetFuseClockPeriodNs() const override { return 37; }

    RC ReadFuseBlock(vector<UINT32>* pReadFuses) override;

protected:
    // Program the state of PS Latch before and after burning fuses to control
    // secure fuse power supply
    virtual RC ProgramPSLatch(bool bSwitchOn);

    // Check the default state of PS latch
    virtual RC CheckPSDefaultState();

    // Clear registers for HW fusing
    RC SetupHwFusing() override;

    // Get GPIO pin connected to FUSE_EN
    virtual UINT32 GetFuseEnableGpioNum();

private:
    // GPIO pin to toggle fuse enable
    static const UINT32 FUSE_EN_GPIO_NUM = 19;

    // Turning off PS latch and next turning on PS latch should be longer than 10us
    // Arbitrarily choosing a 100us delay
    static constexpr UINT32 PS18_RESET_DELAY_US = 100;

    // Pointer to test device to perform register operations
    TestDevice* m_pDev;

    // Pointer back to the main fuse object so it can perform
    // common fuse actions (e.g. get the value of disable_sw_override)
    Fuse* m_pFuse;
};
