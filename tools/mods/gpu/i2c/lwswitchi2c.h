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

#include "i2cimpl.h"
#include "core/include/device.h"

//--------------------------------------------------------------------
//! \brief Class to read and write data on an LwSwitch's I2C port
//!
class LwSwitchI2c : public I2cImpl
{
protected:
    LwSwitchI2c() = default;
    virtual ~LwSwitchI2c() = default;

    RC DoDump(const Dev& dev, UINT32 beginOffset, UINT32 endOffset) override;
    RC DoGetI2cDcbDevInfo(I2cDcbDevInfo* pInfo) override;
    RC DoGetPortInfo(PortInfo* pInfo) override;
    RC DoInitialize(Device::LibDeviceHandle libHandle) override;
    RC DoPing(const Dev& dev) override;
    RC DoRead(const Dev& dev, UINT32 offset, UINT32* pData) override;
    RC DoReadArray(const Dev& dev, UINT32 offset, UINT32 numReads, vector<UINT32>* pData) override;
    RC DoReadBlk(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>* pData) override;
    RC DoReadSequence(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>* pData) override;
    RC DoTestPort(const Dev& dev) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoWrite(const Dev& dev, UINT32 offset, UINT32 data) override;
    RC DoWriteArray(const Dev& dev, UINT32 offset, const vector<UINT32>& data) override;
    RC DoWriteBlk(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>& data) override;
    RC DoWriteSequence(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>& data) override;

private:
    RC DoWriteOrRead(const Dev& dev, bool isWrite, bool isBlk, UINT32 offset, vector<UINT08>* pData);
    RC GetFlags(const Dev& dev, bool isWrite, bool isBlk, UINT32* pFlags) const;

    Device::LibDeviceHandle m_LibHandle = Device::ILWALID_LIB_HANDLE;
};
