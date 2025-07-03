/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "device/interface/i2c.h"

class TestDevice;

class I2cImpl : public I2c
{
protected:
    I2cImpl() = default;
    virtual ~I2cImpl() = default;

    RC DoFind(vector<Dev>* pDevices) override;
    string DoGetName() const override;

protected:
    virtual TestDevice* GetDevice() = 0;
    virtual const TestDevice* GetDevice() const = 0;
};
