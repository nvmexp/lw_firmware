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

#include "lwswitchfuseaccessor.h"
#include "gpu/fuse/fusesrc.h"
#include "gpu/fuse/fuseutils.h"

class Fuse;

class LrFuseAccessor : public LwSwitchFuseAccessor
{
public:
    LrFuseAccessor(TestDevice *pDev);
    virtual ~LrFuseAccessor() = default;

    void SetParent(Fuse* pFuse) override;

    RC SetupFuseBlow() override;
    RC CleanupFuseBlow() override;

    RC BlowFuses(const vector<UINT32>& fuseBinary) override;
    RC SetSwFuses(const map<UINT32, UINT32>& swFuseWrites) override;
    RC SetSwIffFuses(const vector<UINT32>& iffRows, UINT32 iffCrc) override;
    RC VerifySwIffFusing(const vector<UINT32>& iffRows, UINT32 iffCrc) override
        { return RC::OK; }

    UINT32 GetOptFuseValue(UINT32 address) override;

    bool IsSwFusingAllowed() override;

    RC SwReset() override;

    RC ReadFuseBlock(vector<UINT32>* pReadFuses) override;

protected:
    // Set GPIO pin to output value
    virtual void GpioSetOutput(UINT32 gpioNum, bool toHigh);

    // Get GPIO pin connected to FUSE_EN
    virtual UINT32 GetFuseEnableGpioNum();

private:
    // GPIO pin to toggle fuse enable
    static const UINT32 FUSE_EN_GPIO_NUM = 19;

    // Pointer to test device to perform register operations
    TestDevice* m_pTestDev;

    // Pointer back to the main fuse object so it can perform
    // common fuse actions (e.g. get the value of disable_sw_override)
    Fuse* m_pFuse;

    // Poll until IFF fusing status is idle
    RC PollSwIffIdle();

    // Clear registers for HW fusing
    RC SetupHwFusing();
};
