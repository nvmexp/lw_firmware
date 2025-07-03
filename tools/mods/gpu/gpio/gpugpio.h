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

#include "gpioimpl.h"

class GpuSubdevice;

class GpuGpio : public GpioImpl
{
protected:
    GpuGpio();
    virtual ~GpuGpio() = default;

    bool DoGetDirection(UINT32 pinNum) override;
    bool DoGetInput(UINT32 pinNum) override;
    bool DoGetOutput(UINT32 pinNum) override;
    RC DoIncrementCounter(UINT32 key) override;
    RC DoSetActivityLimit
    (
        UINT32    pinNum,
        Direction dir,
        UINT32    maxNumOclwrances,
        bool      disableWhenMaxedOut
    ) override;
    RC DoSetDirection(UINT32 pinNum, bool toOutput) override;
    RC DoSetInterruptNotification(UINT32 pinNum, Direction dir, bool toEnable) override;
    RC DoSetOutput(UINT32 pinNum, bool toHigh) override;
    RC DoShutdown() override;
    RC DoStartErrorCounter() override;
    RC DoStopErrorCounter() override;

private:
    GpuSubdevice* GetGpuSubdevice();
    const GpuSubdevice* GetGpuSubdevice() const;

    static void GpioNotificationHandler(void* Args);
    GpioActivity* GetActivityFromKey(UINT32 Key);

    map<UINT32, GpioActivity> m_NotificationList;
    GpioChecker m_ActivityChecker;
    map<UINT32, GpioChecker::GpioCheckInfo> m_CheckInfo;
};
