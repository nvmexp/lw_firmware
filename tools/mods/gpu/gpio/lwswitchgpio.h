/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpioimpl.h"

class LwSwitchGpio : public GpioImpl
{
protected:
    LwSwitchGpio() = default;
    virtual ~LwSwitchGpio() = default;

    bool DoGetDirection(UINT32 pinNum) override;
    bool DoGetInput(UINT32 pinNum) override;
    bool DoGetOutput(UINT32 pinNum) override;
    RC DoIncrementCounter(UINT32 key) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoSetActivityLimit
    (
        UINT32    pinNum,
        Direction dir,
        UINT32    maxNumOclwrances,
        bool      disableWhenMaxedOut
    ) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoSetDirection(UINT32 pinNum, bool toOutput) override;
    RC DoSetInterruptNotification(UINT32 pinNum, Direction dir, bool toEnable) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoSetOutput(UINT32 pinNum, bool toHigh) override;
    RC DoShutdown() override
        { return RC::OK; }
    RC DoStartErrorCounter() override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoStopErrorCounter() override
        { return RC::UNSUPPORTED_FUNCTION; }
};
