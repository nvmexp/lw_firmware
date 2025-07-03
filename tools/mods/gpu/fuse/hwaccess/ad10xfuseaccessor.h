/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "ga10xfuseaccessor.h"

class AD10xFuseAccessor : public GA10xFuseAccessor
{
public:
    AD10xFuseAccessor(TestDevice* pSubdev);
    virtual ~AD10xFuseAccessor() = default;

    void SetParent(Fuse* pFuse) override;

private:
    // Program the state of PS Latch before and after burning fuses to control
    // secure fuse power supply
    RC ProgramPSLatch(bool bSwitchOn) override;

    // Check the default state of PS latch
    RC CheckPSDefaultState() override;

    // Turning off PS09 and next turning on PS09 should be longer than 10us
    // Arbitrarily choosing a 100us delay
    static constexpr UINT32 PS09_RESET_DELAY_US = 100;

    // Pointer to test device to perform register operations
    TestDevice* m_pDev;

    // Pointer back to the main fuse object so it can perform
    // common fuse actions (e.g. get the value of disable_sw_override)
    Fuse* m_pFuse;
};
