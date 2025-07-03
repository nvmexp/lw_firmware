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

#ifndef INCLUDED_STOP_WATCH_H
#define INCLUDED_STOP_WATCH_H

#include "types.h"

// A simple stopwatch based on Platform::GetSimulatorTime.
class StopWatch {
public:
    StopWatch();

    void Reset(); // clears the content of the stop watch
    void Start(); // start a time measurement
    void Stop(); // stop a time measurement

    // obtain the current elapsed time, in seconds
    double GetElapsedTimeInSeconds() const;

private:
    // time stamp counter value at time of start
    uint64_t m_startClock;

    // cumulative clocks expired since start of time measurement
    uint64_t m_elapsedClocks;
};

#endif // INCLUDED_STOP_WATCH_H
