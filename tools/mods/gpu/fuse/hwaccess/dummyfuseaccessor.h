/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019,2021 by LWPU Corporation.  All rights reserved.  All
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
class TestDevice;

class DummyFuseAccessor : public FuseHwAccessor
{
public:
    DummyFuseAccessor(TestDevice* pDev, UINT32 numRows);
    virtual ~DummyFuseAccessor() = default;

    void SetParent(Fuse* pFuse) override;

    RC SetupFuseBlow() override;
    RC CleanupFuseBlow() override;

    RC BlowFuses(const vector<UINT32>& fuseBinary) override;
    RC SetSwFuses(const map<UINT32, UINT32>& swFuseWrites) override;
    RC SetSwIffFuses(const vector<UINT32>& iffRows, UINT32 iffCrc) override;
    RC VerifySwIffFusing(const vector<UINT32>& iffRows, UINT32 iffCrc) override;

    RC GetFuseRow(UINT32 row, UINT32* pValue) override;
    UINT32 GetOptFuseValue(UINT32 address) override;

    void DisableWaitForIdle() override;

    bool IsFusePrivSelwrityEnabled() override;
    bool IsSwFusingAllowed() override;

    RC LatchFusesToOptRegs() override;

    RC SwReset() override;

    RC GetNumFuseRows(UINT32 *pNumFuseRows) const override;

    UINT32 GetFuseClockPeriodNs() const override
        { return 0; }

    RC SetFusingVoltage(UINT32 vddMv) override;

protected:
    RC ReadFuseBlock(vector<UINT32>* pReadFuses) override;

    RC VerifyFusing(const vector<UINT32>& expectedBinary);
private:
    // Pointer back to the main fuse object
    Fuse* m_pFuse;

    // Pointer to test device
    TestDevice* m_pDev;

    // Dummy fuse macro
    vector<UINT32> m_HwFuses;

    // Dummy SW fuses
    map<UINT32, UINT32> m_SwFuses;

    // Dummy SW IFF RAM
    vector<UINT32> m_SwIffRam;

    // Num of rows in the HW macro
    UINT32 m_NumRows;

    // SW fuses were initialized
    bool m_SwFusesInitialized;

    // Set reset values for SW fuses
    RC InitSwFuses();
};
