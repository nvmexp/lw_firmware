/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2013,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file powermgmtutil.h
//! \brief Power management utility header used by power related tests
//!

#ifndef INCLUDED_RMT_POWERMGMTUTIL_H
#define INCLUDED_RMT_POWERMGMTUTIL_H

#include "core/include/channel.h"
#include "core/include/xp.h"
#include "lwRmApi.h"
#include "Lwcm.h"
#include "core/include/utility.h"
#include "core/include/platform.h"
#include "gpu/tests/gputest.h"

class GpuSubdevice;

class PowerMgmtUtil
{
public:
    enum POWER_STATE
    {
        POWER_S1,
        POWER_S2,
        POWER_S3,
        POWER_S4
    };

    enum GPU_PWR_ACTION
    {
        GPU_PWR_SUSPEND,
        GPU_PWR_RESUME,
        GPU_PWR_RESET,
        GPU_PWR_FB_DIRTY_PARTIAL,
        GPU_PWR_FB_DRITY_FULL
    };

    //
    // Member functions
    //
    PowerMgmtUtil();
    ~PowerMgmtUtil();

    //
    // Tell this object which GpuSubdevice object to use.
    //
    RC BindGpuSubdevice(GpuSubdevice * pGpuSubdev);

    //
    // PowerStateTransition puts the entire system into s-state
    // by calling o/s function.
    //
    RC PowerStateTransition(POWER_STATE ePowerState,
                            INT32 iSleepTimeInSec);

    //
    // RmSetPowerState directly calls RM function to put just the
    // GPU into s-state. This is designed to test under simulation.
    // Risk of calling this on o/s will end up display driver state corrupted.
    //
    RC RmSetPowerState(GPU_PWR_ACTION gpuState);

    //
    // SuspendResumeCycle can be used to Suspend and after specified time
    // interval, Resume the system.
    //
    RC GoToSuspendAndResume(UINT32 iSleepTimeInSec);

    LwRm::Handle GetSubDeviceDiag();

private:
    LwRm::Handle                             m_hSubDev;
    LwRm::Handle                             m_hSubDevDiag;

};

#endif // INCLUDED_RMT_POWERMGMTUTIL_H
