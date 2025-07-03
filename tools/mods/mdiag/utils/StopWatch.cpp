/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2001-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "StopWatch.h"
#include "core/include/platform.h"

StopWatch::StopWatch()
{
    m_startClock = 0;
    m_elapsedClocks = 0;
}

void StopWatch::Reset()
{
    m_startClock = 0;
    m_elapsedClocks = 0;
}

void StopWatch::Start()
{
    m_startClock = Platform::GetSimulatorTime();
}

void StopWatch::Stop()
{
    uint64_t stopClock = Platform::GetSimulatorTime();
    int64_t deltaClocks = stopClock - m_startClock;
    m_elapsedClocks += deltaClocks;
}

double StopWatch::GetElapsedTimeInSeconds() const
{
    double secsPerTick = Platform::GetSimulatorTimeUnitsNS() * 1e-9;
    return double(m_elapsedClocks) * secsPerTick;
}
