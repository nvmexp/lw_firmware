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

#include "gpugpio.h"
#include "mods_reg_hal.h"
#include "gpu/reghal/reghal.h"
#include "gpu/include/testdevice.h"
#include "gpu/include/gpusbdev.h"
#include "ctrl/ctrl2080.h"
#include "class/cl208f.h" // LW20_SUBDEVICE_DIAG

//-----------------------------------------------------------------------------
GpuGpio::GpuGpio()
: m_ActivityChecker(this)
{
}

//-----------------------------------------------------------------------------
bool GpuGpio::DoGetDirection(UINT32 pinNum)
{
    return GetDevice()->Regs().Read32(MODS_PMGR_GPIO_OUTPUT_CNTL_IO_OUT_EN, pinNum);
}

//-----------------------------------------------------------------------------
bool GpuGpio::DoGetInput(UINT32 pinNum)
{
    return GetGpuSubdevice()->GpioGetInput(pinNum);
}

//-----------------------------------------------------------------------------
bool GpuGpio::DoGetOutput(UINT32 pinNum)
{
    return GetGpuSubdevice()->GpioGetOutput(pinNum);
}

//-----------------------------------------------------------------------------
RC GpuGpio::DoIncrementCounter(UINT32 key)
{
    return m_ActivityChecker.Update(key);
}

//-----------------------------------------------------------------------------
RC GpuGpio::DoSetActivityLimit
(
    UINT32    pinNum,
    Direction dir,
    UINT32    maxNumOclwrances,
    bool      disableWhenMaxedOut
)
{
    UINT32 key = ActivityToKey(pinNum, dir);
    if (m_NotificationList.find(key) == m_NotificationList.end())
    {
        Printf(Tee::PriError, "The GPIO num and edge was not registered\n");
        return RC::BAD_PARAMETER;
    }
    GpioChecker::GpioCheckInfo newCheck;
    newCheck.maxAllowed            = maxNumOclwrances;
    newCheck.disableWhenMaxReached = disableWhenMaxedOut;
    m_CheckInfo[key]               = newCheck;

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC GpuGpio::DoSetDirection(UINT32 pinNum, bool toOutput)
{
    GetDevice()->Regs().Write32(MODS_PMGR_GPIO_OUTPUT_CNTL_IO_OUT_EN, pinNum, toOutput ? 1 : 0);
    return RC::OK;
}

//-----------------------------------------------------------------------------
/* static */ void GpuGpio::GpioNotificationHandler(void* args)
{
    MASSERT(args);
    GpioImpl::GpioActivity *pAct = static_cast<GpioActivity*>(args);
    Gpio *pGpio = pAct->pGpio;
    Printf(Tee::PriLow, "Gpio %d: RmEventId: %d\n", pAct->pinNum, pAct->rmEventId);
    UINT32 key = ActivityToKey(pAct->pinNum, pAct->dir);
    pGpio->IncrementCounter(key);
}

//-----------------------------------------------------------------------------
RC GpuGpio::DoSetInterruptNotification(UINT32 pinNum, Direction dir, bool toEnable)
{
    RC rc;
    GpioActivity activity;
    activity.dir       = dir;
    activity.pinNum    = pinNum;
    activity.pGpio     = this;

    LW208F_CTRL_PMGR_GPIO_INTERRUPT_NOTIFICATION_PARAMS params;
    params.gpioPin  = pinNum;
    params.isEnable = toEnable;
    switch (dir)
    {
        case RISING:
            params.direction = 1;
            activity.rmEventId = LW2080_NOTIFIERS_GPIO_RISING_INTERRUPT(pinNum);
            break;
        case FALLING:
            params.direction = 0;
            activity.rmEventId = LW2080_NOTIFIERS_GPIO_FALLING_INTERRUPT(pinNum);
            break;
        default:
            return RC::BAD_PARAMETER;
    }

    UINT32 key = ActivityToKey(pinNum, dir);

    if (toEnable)
    {
        if (m_NotificationList.find(key) == m_NotificationList.end())
        {
            m_NotificationList[key] = activity;
            CHECK_RC(GetGpuSubdevice()->HookResmanEvent(
                        activity.rmEventId,
                        GpioNotificationHandler,
                        &m_NotificationList[key],
                        LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT,
                        GpuSubdevice::NOTIFIER_MEMORY_DISABLED));
            Printf(Tee::PriDebug, "GPIO notification (%d) added\n",
                   activity.rmEventId);
        }
        else
        {
            Printf(Tee::PriNormal, "GPIO notification already set\n");
            return RC::OK;
        }
    }
    else
    {
        if (m_NotificationList.find(key) == m_NotificationList.end())
        {
            Printf(Tee::PriError, "GPIO notification was never set\n");
            return RC::BAD_PARAMETER;
        }
        else
        {
            CHECK_RC(GetGpuSubdevice()->UnhookResmanEvent(activity.rmEventId));
            m_NotificationList.erase(key);
            Printf(Tee::PriDebug, "GPIO notification (%d) removed\n",
                   activity.rmEventId);
        }
    }

    CHECK_RC(LwRmPtr()->Control(
                    GetGpuSubdevice()->GetSubdeviceDiag(),
                    LW208F_CTRL_CMD_PMGR_SET_GPIO_INTERRUPT_NOTIFICATION,
                    &params, sizeof(params)));
    return rc;
}


//-----------------------------------------------------------------------------
RC GpuGpio::DoSetOutput(UINT32 pinNum, bool toHigh)
{
    GetGpuSubdevice()->GpioSetOutput(pinNum, toHigh);
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC GpuGpio::DoStartErrorCounter()
{
    return m_ActivityChecker.Initialize(m_CheckInfo);
}

//-----------------------------------------------------------------------------
RC GpuGpio::DoShutdown()
{
    StickyRC rc;
    // unhook anything that's still wired
    for (auto notifyIter = m_NotificationList.begin(); notifyIter != m_NotificationList.end(); notifyIter++)
    {
        rc = GetGpuSubdevice()->UnhookResmanEvent(notifyIter->second.rmEventId);
    }
    m_NotificationList.clear();
    return rc;
}

//-----------------------------------------------------------------------------
RC GpuGpio::DoStopErrorCounter()
{
    return m_ActivityChecker.ShutDown(true);
}

//-----------------------------------------------------------------------------
GpioImpl::GpioActivity* GpuGpio::GetActivityFromKey(UINT32 key)
{
    if (m_NotificationList.find(key) == m_NotificationList.end())
    {
        return nullptr;
    }
    return &m_NotificationList[key];
}

//-----------------------------------------------------------------------------
GpuSubdevice* GpuGpio::GetGpuSubdevice()
{
    return GetDevice()->GetInterface<GpuSubdevice>();
}

//-----------------------------------------------------------------------------
const GpuSubdevice* GpuGpio::GetGpuSubdevice() const
{
    return GetDevice()->GetInterface<GpuSubdevice>();
}
