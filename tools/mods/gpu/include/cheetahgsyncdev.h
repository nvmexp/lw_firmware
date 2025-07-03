/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <vector>
#include "gpu/display/tegra_disp.h"
#include "gpu/include/gsyncdev.h"
#include "cheetah/include/sysutil.h"

class GpuSubdevice;

// TegraGsyncDevice Class
//
// Interface for communicating with a Gsync device
//
class TegraGsyncDevice : public GsyncDevice
{
public:
    TegraGsyncDevice
    (
        Display *pDisplay
        ,GpuSubdevice *pSubdev
        ,UINT32 displayMask
        ,GsyncSink::Protocol protocol
        ,Display::I2CPortNum i2cPort
    );
    virtual ~TegraGsyncDevice() {}

    static RC FindTegraGsyncDevices
    (
        TegraDisplay *pDisplay,
        GpuSubdevice *pSubdev,
        vector<TegraGsyncDevice> *pGsyncDevices
    );

    static RC PowerToggleHDMI
    (
        TegraDisplay *pDisplay,
        UINT32 value
    );

    const SysUtil::I2c& GetI2cDevice() const
    {
        return m_SysI2cDev;
    }

    Display::I2CPortNum GetI2cPortNum() const
    {
        return m_I2cPort;
    }

private:
    SysUtil::I2c m_SysI2cDev;
    static UINT08 s_HDMIPowerToggleValue;
    Display::I2CPortNum m_I2cPort;

    RC GetDpSymbolErrors
    (
        UINT32 *pTotalErrors,
        vector<UINT16> *pPerLaneErrors
    ) override;

    RC AccessGsyncSink
    (
        GsyncSink::TMDS           type,
        Display::DpAuxTransaction command,
        UINT08* const             pBuffer,
        const UINT32              numElems,
        Display::DpAuxRetry       enableRetry //Don't care
    ) override;

    RC GetCrcSignature(string *pCrcSignature) override;
};
