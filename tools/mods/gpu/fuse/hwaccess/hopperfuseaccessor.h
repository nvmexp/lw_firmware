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

class HopperFuseAccessor : public GA10xFuseAccessor
{
public:
    HopperFuseAccessor(TestDevice* pSubdev);
    virtual ~HopperFuseAccessor() = default;

    void SetParent(Fuse* pFuse) override;

    RC BlowFuses(const vector<UINT32>& fuseBinary) override;

    RC SetSwIffFuses(const vector<UINT32>& iffRows, UINT32 iffCrc) override;

    RC VerifySwIffFusing(const vector<UINT32>& iffRows, UINT32 iffCrc) override;

    RC SetFusingVoltage(UINT32 vddMv) override;

    RC ReadFuseBlock(vector<UINT32>* pReadFuses) override;

protected:
    // Verify that what we tried to burn into the fuses actually made it
    RC VerifyFusing(const vector<UINT32>& expectedBinary) override;

    // Clear registers for HW fusing
    RC SetupHwFusing() override;

    // Program the state of PS Latch before and after burning fuses to control
    // secure fuse power supply
    RC ProgramPSLatch(bool bSwitchOn) override;

    // Check the default state of PS latch
    RC CheckPSDefaultState() override;

    // Get GPIO pin connected to FUSE_EN
    UINT32 GetFuseEnableGpioNum() override;

private:
    // Get section of the fuse macro that is read protected
    void GetReadProtectedRows(UINT32 *pStart, UINT32 *pEnd);

    // GPIO pin to toggle fuse enable
    // See https://lwbugswb.lwpu.com/LWBugs5/redir.aspx?url=/3423127/22
    static constexpr UINT32 FUSE_EN_GPIO_NUM = 11;

    // Turning off PS09 and next turning on PS09 should be longer than 10us
    // Arbitrarily choosing a 100us delay
    static constexpr UINT32 PS09_RESET_DELAY_US = 100;

    // Maximum number of IFF record rows in the sequence RAM
    static constexpr UINT32 MAX_IFF_ROWS = 128;

    // Pointer to test device to perform register operations
    TestDevice* m_pDev;

    // Pointer back to the main fuse object so it can perform
    // common fuse actions (e.g. get the value of disable_sw_override)
    Fuse* m_pFuse;
};
