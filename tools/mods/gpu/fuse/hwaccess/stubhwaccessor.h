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

class StubFuseHwAccessor : public FuseHwAccessor
{
public:
    virtual ~StubFuseHwAccessor() = default;

    virtual void SetParent(Fuse* pFuse) override { }

    virtual RC SetupFuseBlow() override
        { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC CleanupFuseBlow() override
        { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC BlowFuses(const vector<UINT32>& fuseBinary) override
        { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC SetSwFuses(const map<UINT32, UINT32>& swFuseWrites) override
        { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC SetSwIffFuses(const vector<UINT32>& iffRows, UINT32 iffCrc) override
        { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC VerifySwIffFusing(const vector<UINT32>& iffRows, UINT32 iffCrc) override
        { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC GetFuseRow(UINT32 row, UINT32* pValue) override
        { return RC::UNSUPPORTED_FUNCTION; }

    virtual UINT32 GetOptFuseValue(UINT32 address) override
        { return 0; }

    virtual void DisableWaitForIdle() override {}

    virtual bool IsFusePrivSelwrityEnabled() override
        { return false; }

    virtual bool IsSwFusingAllowed() override
        { return false; }

    virtual RC LatchFusesToOptRegs() override
        { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC SwReset() override
        { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC GetNumFuseRows(UINT32 *pNumFuseRows) const override
        { return 0; }

    virtual UINT32 GetFuseClockPeriodNs() const override
        { return 0; }

    virtual RC SetFusingVoltage(UINT32 vddMv) override
        { return RC::UNSUPPORTED_FUNCTION; }

protected:
    virtual RC ReadFuseBlock(vector<UINT32>* pReadFuses) override
        { return RC::UNSUPPORTED_FUNCTION; }
};
