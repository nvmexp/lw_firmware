/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/fuse/fusehwaccess.h"
#include "gpu/fuse/fusesrc.h"
#include "gpu/fuse/fuseutils.h"

class Fuse;

class LwSwitchFuseAccessor : public FuseHwAccessor
{
public:
    LwSwitchFuseAccessor(TestDevice *pDev);
    virtual ~LwSwitchFuseAccessor() = default;

    void SetParent(Fuse* pFuse) override;

    RC SetupFuseBlow() override;
    RC CleanupFuseBlow() override;

    RC BlowFuses(const vector<UINT32>& fuseBinary) override;
    RC SetSwFuses(const map<UINT32, UINT32>& swFuseWrites) override;
    RC SetSwIffFuses(const vector<UINT32>& iffRows, UINT32 iffCrc) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC VerifySwIffFusing(const vector<UINT32>& iffRows, UINT32 iffCrc) override
        { return RC::UNSUPPORTED_FUNCTION; }

    RC GetFuseRow(UINT32 row, UINT32* pValue) override;
    UINT32 GetOptFuseValue(UINT32 address) override;

    void DisableWaitForIdle() override;

    bool IsFusePrivSelwrityEnabled() override;
    bool IsSwFusingAllowed() override;

    RC GetNumFuseRows(UINT32 *pNumFuseRows) const override;

    UINT32 GetFuseClockPeriodNs() const override
        { return 0; }

    // Trigger the FUSE hardware to re-read fuses and update the OPT registers
    RC LatchFusesToOptRegs() override;

    RC SwReset() override
    {
        return RC::OK;
    }

    RC SetFusingVoltage(UINT32 vddMv) override
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

protected:
    RC ReadFuseBlock(vector<UINT32>* pReadFuses) override;

    // Poll the FUSE hardware until it reports IDLE
    RC PollFusesIdle();

    // Verify that what we tried to burn into the fuses actually made it
    virtual RC VerifyFusing(const vector<UINT32>& expectedBinary);

private:
    // Pointer to test device to perform register operations
    TestDevice* m_pTestDev;

    // Pointer back to the main fuse object so it can perform
    // common fuse actions (e.g. get the value of sw_override_fuse)
    Fuse *m_pFuse = nullptr;

    // Wait for FUSECTRL to report STATUS_IDLE before continuing fuse operations
    bool m_WaitForIdle = true;

    // Set the daughter card to output high/low
    RC EnableFuseSrc(bool setHigh);
};

