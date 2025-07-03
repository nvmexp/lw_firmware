/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "i2cimpl.h"

//--------------------------------------------------------------------
//! \brief Class to read and write data on a Simulated device's I2C port
//!
class SimI2c : public I2cImpl
{
protected:
    SimI2c() = default;
    virtual ~SimI2c() = default;

    RC DoDump(const Dev& dev, UINT32 beginOffset, UINT32 endOffset) override
        { return RC::OK; }
    RC DoGetI2cDcbDevInfo(I2cDcbDevInfo* pInfo) override
        { MASSERT(pInfo); pInfo->clear(); return RC::OK; }
    RC DoGetPortInfo(PortInfo* pInfo) override
        { MASSERT(pInfo); pInfo->clear(); return RC::OK; }
    RC DoInitialize(LwLinkDevIf::LibInterface::LibDeviceHandle libHandle) override
        { return RC::OK; }
    RC DoPing(const Dev& dev) override
        { return RC::OK; }
    RC DoRead(const Dev& dev, UINT32 offset, UINT32* pData) override
        { MASSERT(pData); *pData = 0; return RC::OK; }
    RC DoReadArray(const Dev& dev, UINT32 offset, UINT32 numReads, vector<UINT32>* pData) override
        { return RC::OK; }
    RC DoReadBlk(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>* pData) override
        { return RC::OK; }
    RC DoReadSequence(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>* pData) override
        { return RC::OK; }
    RC DoTestPort(const Dev& dev) override
        { return RC::OK; }
    RC DoWrite(const Dev& dev, UINT32 offset, UINT32 data) override
        { return RC::OK; }
    RC DoWriteArray(const Dev& dev, UINT32 offset, const vector<UINT32>& data) override
        { return RC::OK; }
    RC DoWriteBlk(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>& data) override
        { return RC::OK; }
    RC DoWriteSequence(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>& data) override
        { return RC::OK; }

};
