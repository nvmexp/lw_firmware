/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2011,2013,2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file powermgmtutil.cpp
//! \brief Power management utility class implementation
//!

#include "gpu/perf/powermgmtutil.h"
#include "ctrl/ctrl208f.h" // LW20_SUBDEVICE_DIAG CTRL
#include "core/include/platform.h"
#include "core/include/refparse.h"
#include "gpu/include/gpusbdev.h"
#include "lwos.h"
#include "lwRmApi.h"
#include "core/include/tasker.h"

//! \brief PowerMgmtUtil
//!
//! Power management util constructor
//!
//-----------------------------------------------------------------------------
PowerMgmtUtil::PowerMgmtUtil()
{
    m_hSubDev = 0;
    m_hSubDevDiag = 0;
}

//! \brief PowerMgmtUtil
//!
//! Power management util destructor
//!
//-----------------------------------------------------------------------------
PowerMgmtUtil::~PowerMgmtUtil()
{
}

//! \brief Use this GpuSubdevice.
RC PowerMgmtUtil::BindGpuSubdevice(GpuSubdevice * pGpuSubdev)
{
    RC rc;
    // Get the new subdevice handle to which we're binding the utility object
    LwRm::Handle newSubDeviceHandle = LwRmPtr()->GetSubdeviceHandle(pGpuSubdev);
    m_hSubDev = newSubDeviceHandle;
    m_hSubDevDiag = pGpuSubdev->GetSubdeviceDiag();

    return rc;
}

//! \brief PowerStateTransition
//!
//! The util function which encapsulates all the power state transitions.
//! Some examples like goto suspend state, change power mizer state.
//!
//! \return RC -> OK if everything as expected, specific RC if something
//!         failed.
//-----------------------------------------------------------------------------
RC PowerMgmtUtil::PowerStateTransition(POWER_STATE ePowerState,
                                       INT32 iSleepTimeInSec)
{
    RC rc;

    switch (ePowerState)
    {
        case POWER_S1:
        case POWER_S2:
        case POWER_S3:
        {
            //true == standby
            Printf(Tee::PriLow,
                   " %d: PowerStateTransition:This is Standby call\n",
                   __LINE__);
            CHECK_RC(Xp::SwitchSystemPowerState(true, iSleepTimeInSec));
            break;
        }

        case POWER_S4:
        {
            //false == Hibernate
            Printf(Tee::PriLow,
                   " %d: PowerStateTransition:This is Hibernate call\n",
                   __LINE__);
            CHECK_RC(Xp::SwitchSystemPowerState(false, iSleepTimeInSec));
        }

        default: // Do nothing
            break;
    }
    return rc;
}

RC PowerMgmtUtil::RmSetPowerState(GPU_PWR_ACTION gpuAction)
{
    RC rc;
    LW208F_CTRL_SUSPEND_RESUME_QUICK_PARAMS SuspendResumeParams = {0};

    switch (gpuAction)
    {
        case GPU_PWR_SUSPEND:
            SuspendResumeParams.srAction = LW208F_CTRL_CMD_SUSPEND_RESUME_ACTION_SUSPEND;
            break;
        case GPU_PWR_RESUME:
            SuspendResumeParams.srAction = LW208F_CTRL_CMD_SUSPEND_RESUME_ACTION_RESUME;
            break;
        case GPU_PWR_FB_DIRTY_PARTIAL:
            SuspendResumeParams.srAction = LW208F_CTRL_CMD_SUSPEND_RESUME_ACTION_FB_DIRTY_PARTIAL;
            break;
        case GPU_PWR_FB_DRITY_FULL:
            SuspendResumeParams.srAction = LW208F_CTRL_CMD_SUSPEND_RESUME_ACTION_FB_DIRTY_FULL;
            break;
        case GPU_PWR_RESET:
            CHECK_RC(Platform::Reset());
            // Here we unload/load fmodel only, so get out now.
            return rc;
            break;
        default:
            return RC::ILWALID_INPUT;
            break;

    }
    rc = LwRmPtr()->Control(m_hSubDevDiag,
                         LW208F_CTRL_CMD_SUSPEND_RESUME_QUICK,
                         &SuspendResumeParams,
                         sizeof (SuspendResumeParams));

    return rc;
}

RC PowerMgmtUtil::GoToSuspendAndResume(UINT32 iSleepTimeInSec)
{
    RC rc;

    if (Platform::GetSimulationMode() == Platform::Fmodel) //SIM
    {
        CHECK_RC(RmSetPowerState(GPU_PWR_SUSPEND));

        // Wait for specified seconds before calling Resume
        Tasker::Sleep(iSleepTimeInSec*1000);

        CHECK_RC(RmSetPowerState(GPU_PWR_RESUME));
    }
    else //HW
    {
        CHECK_RC(PowerStateTransition(POWER_S3, iSleepTimeInSec));
    }
    return rc;
}

LwRm::Handle PowerMgmtUtil::GetSubDeviceDiag()
{
    return m_hSubDevDiag;
}

