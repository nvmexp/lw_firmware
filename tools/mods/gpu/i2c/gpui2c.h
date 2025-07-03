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
#include "core/include/lwrm.h"
#include "core/include/device.h"

class GpuSubdevice;

//--------------------------------------------------------------------
//! \brief Class to read and write data on a GPU's I2C port
//!
class GpuI2c : public I2cImpl
{
protected:
    GpuI2c() = default;
    virtual ~GpuI2c() = default;

    RC DoDump(const Dev& dev, UINT32 beginOffset, UINT32 endOffset) override;
    RC DoGetI2cDcbDevInfo(I2cDcbDevInfo* pInfo) override;
    RC DoGetPortInfo(PortInfo* pInfo) override;
    RC DoInitialize(Device::LibDeviceHandle libHandle) override
        { return OK; }
    RC DoPing(const Dev& dev) override;
    RC DoRead(const Dev& dev, UINT32 offset, UINT32* pData) override;
    RC DoReadArray(const Dev& dev, UINT32 offset, UINT32 numReads, vector<UINT32>* pData) override;
    RC DoReadBlk(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>* pData) override;
    RC DoReadSequence(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>* pData) override;
    RC DoTestPort(const Dev& dev) override;
    RC DoWrite(const Dev& dev, UINT32 offset, UINT32 data) override;
    RC DoWriteArray(const Dev& dev, UINT32 offset, const vector<UINT32>& data) override;
    RC DoWriteBlk(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>& data) override;
    RC DoWriteSequence(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>& data) override;

private:
    enum class I2cReadWrite : UINT08
    {
        READ,
        WRITE
    };
    GpuSubdevice* GetGpuSubdevice();
    RC DoWriteOrRead(const Dev& dev, bool isWrite, UINT32 offset, vector<UINT08>* pData,
                     LwRm::Handle i2cHandle);
    RC DoBlkWriteOrRead(const Dev& dev, I2cReadWrite RdWr, UINT32 Offset, UINT32 NumBytes,
                        vector<UINT08> *pData,
                        LwRm::Handle I2cHandle);
    UINT32 GetFlags(const Dev& dev) const;
};
