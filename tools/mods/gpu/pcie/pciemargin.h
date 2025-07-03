/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"

class PcieMargin
{
public:
    virtual ~PcieMargin() = default;

    struct CtrlCaps
    {
        bool bIndErrorSampler = false;
        bool bIndLeftRightTiming = false;
        bool bIndUpDowlwoltage = false;
        bool bVoltageSupported = false;
    };
    enum TimingMarginDir
    {
        TMD_RIGHT = 0,
        TMD_LEFT = 1,
        TMD_UNIDIR = 0xf
    };
    enum VoltageMarginDir
    {
        VMD_UP = 0,
        VMD_DOWN = 1,
        VMD_UNIDIR = 0xf
    };

    RC ClearMarginErrorLog(UINT32 lane)
        { return DoClearMarginErrorLog(lane); }
    RC DisableMargining()
        { return DoDisableMargining(); }
    RC EnableMargining()
        { return DoEnableMargining(); }
    RC GetMarginCtrlCaps(UINT32 lane, CtrlCaps* pCaps)
        { return DoGetMarginCtrlCaps(lane, pCaps); }
    RC GetMarginMaxTimingOffset(UINT32 lane, UINT32* pOffset)
        { return DoGetMarginMaxTimingOffset(lane, pOffset); }
    RC GetMarginMaxVoltageOffset(UINT32 lane, UINT32* pOffset)
        { return DoGetMarginMaxVoltageOffset(lane, pOffset); }
    RC GetMarginNumTimingSteps(UINT32 lane, UINT32* pSteps)
        { return DoGetMarginNumTimingSteps(lane, pSteps); }
    RC GetMarginNumVoltageSteps(UINT32 lane, UINT32* pSteps)
        { return DoGetMarginNumVoltageSteps(lane, pSteps); }
    RC SetMarginErrorCountLimit(UINT32 lane, UINT32 limit)
        { return DoSetMarginErrorCountLimit(lane, limit); }
    RC SetMarginNormalSettings(UINT32 lane)
        { return DoSetMarginNormalSettings(lane); }
    RC SetMarginTimingOffset(UINT32 lane, TimingMarginDir dir, UINT32 offset, FLOAT64 timeoutMs, bool* pVerified)
        { return DoSetMarginTimingOffset(lane, dir, offset, timeoutMs, pVerified); }
    RC SetMargilwoltageOffset(UINT32 lane, VoltageMarginDir dir, UINT32 offset, FLOAT64 timeoutMs, bool* pVerified)
        { return DoSetMargilwoltageOffset(lane, dir, offset, timeoutMs, pVerified); }

private:
    virtual RC DoClearMarginErrorLog(UINT32 lane) = 0;
    virtual RC DoDisableMargining() = 0;
    virtual RC DoEnableMargining() = 0;
    virtual RC DoGetMarginCtrlCaps(UINT32 lane, CtrlCaps* pCaps) = 0;
    virtual RC DoGetMarginMaxTimingOffset(UINT32 lane, UINT32* pOffset) = 0;
    virtual RC DoGetMarginMaxVoltageOffset(UINT32 lane, UINT32* pOffset) = 0;
    virtual RC DoGetMarginNumTimingSteps(UINT32 lane, UINT32* pSteps) = 0;
    virtual RC DoGetMarginNumVoltageSteps(UINT32 lane, UINT32* pSteps) = 0;
    virtual RC DoSetMarginErrorCountLimit(UINT32 lane, UINT32 limit) = 0;
    virtual RC DoSetMarginNormalSettings(UINT32 lane) = 0;
    virtual RC DoSetMarginTimingOffset(UINT32 lane, TimingMarginDir dir, UINT32 offset, FLOAT64 timeoutMs, bool* pVerified) = 0;
    virtual RC DoSetMargilwoltageOffset(UINT32 lane, VoltageMarginDir dir, UINT32 offset, FLOAT64 timeoutMs, bool* pVerified) = 0;
};
