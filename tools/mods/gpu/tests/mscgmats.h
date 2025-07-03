/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014,2018,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDE_MSCGMATS_H
#define INCLUDE_MSCGMATS_H

#include "nwfmats.h"
#include "gpu/perf/perfsub.h"

class LowPowerPgBubble;

class MscgMatsTest: public NewWfMatsTest
{
public:
    MscgMatsTest();

    bool IsSupported() override;
    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;
    void PrintJsProperties(Tee::Priority pri) override;

    SETGET_PROP(SleepMs, FLOAT64);
    SETGET_PROP(SleepLoops, UINT32);
    SETGET_PROP(MinSuccessPercent, FLOAT64);
    SETGET_PROP(ForcePstate, UINT32);
    SETGET_PROP(DelayAfterBubbleMs, FLOAT64);

protected:
    RC SubroutineSleep() override;

private:
    Surface2D m_Pixel;
    UINT32 m_PixelPattern = 0x12345678; // Data to write to m_Pixel
    UINT32 m_SleepLoops = 200;
    UINT32 m_MscgEntryAttempts = 0;
    UINT32 m_ForcePstate = Perf::ILWALID_PSTATE;
    FLOAT64 m_MinSuccessPercent = 50.0f;
    FLOAT64 m_SleepMs = 1000.0f;
    FLOAT64 m_DelayAfterBubbleMs = 0;
    Perf::PerfPoint m_RestorePerfPt;
    std::unique_ptr<LowPowerPgBubble> m_LowPowerPgBubble;
};

#endif // INCLUDE_MSCGMATS_H
