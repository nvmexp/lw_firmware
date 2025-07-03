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

#include "core/include/device.h"
#include "core/include/setget.h"

//--------------------------------------------------------------------
//! \brief Interface class to read and write data on a device's I2C port
//!
class I2c
{
public:
    // known types of I2c Devices.. list will grow
    enum I2cDevice
    {
        I2C_INA219,
        I2C_INA3221,
        I2C_ADT7473,
        I2C_ADS1112,
    };
    enum I2cSpeed
    {
        SPEED_100KHz = 100,     // RM default
        SPEED_200KHz = 200,
        SPEED_400KHz = 400,
        SPEED_33KHz = 33,
        SPEED_10KHz = 10,
        SPEED_3KHz = 3,
        SPEED_300KHz = 300,
        SPEED_1000KHz = 1000,
    };
    enum AddressMode
    {
        ADDRMODE_7BIT = 7,      // RM default
        ADDRMODE_10BIT = 10,
    };
    enum I2cFlavor
    {
        FLAVOR_HW = 0,
        FLAVOR_SW = 1
    };

    struct PortInfoEntry
    {
        bool Implemented;
        bool DcbDeclared;
        bool DdcChannel;
        bool CrtcMapped;
    };
    using PortInfo = vector<PortInfoEntry>;
    struct I2cDcbDevInfoEntry
    {
        UINT08 Type;
        UINT16 I2CAddress;
        UINT08 I2CLogicalPort;
        UINT08 I2CDeviceIdx;
    };
    using I2cDcbDevInfo =  vector<I2cDcbDevInfoEntry>;

    static const UINT32 NUM_PORTS;
    static const UINT32 DYNAMIC_PORT;
    static const UINT08 CHIL_WRITE_CMD;
    static const UINT08 CHIL_READ_CMD;
    static const UINT32 NUM_DEV;

    class Dev final
    {
        friend class I2c;
    public:
        ~Dev() = default;
        Dev(const Dev& rhs)
            : m_pI2c(rhs.m_pI2c), m_Port(rhs.m_Port), m_Addr(rhs.m_Addr), m_MsbFirst(rhs.m_MsbFirst)
            , m_OffsetLength(rhs.m_OffsetLength), m_MessageLength(rhs.m_MessageLength)
            , m_SpeedInKHz(rhs.m_SpeedInKHz), m_AddrMode(rhs.m_AddrMode)
            , m_WriteCmd(rhs.m_WriteCmd), m_ReadCmd(rhs.m_ReadCmd)
            , m_VerifyReads(rhs.m_VerifyReads), m_Flavor(rhs.m_Flavor) {}

        RC Dump(UINT32 beginOffset, UINT32 endOffset)
            { return m_pI2c->DoDump(*this, beginOffset, endOffset); }
        RC Ping()
            { return m_pI2c->DoPing(*this); }
        void PrintSpec(Tee::Priority pri) const;
        RC Read(UINT32 offset, UINT32 *pData)
            { return m_pI2c->DoRead(*this, offset, pData); }
        RC ReadArray(UINT32 offset, UINT32 numReads, vector<UINT32>* pData)
            { return m_pI2c->DoReadArray(*this, offset, numReads, pData); }
        RC ReadBlk(UINT32 offset, UINT32 numBytes, vector<UINT08>* pData)
            { return m_pI2c->DoReadBlk(*this, offset, numBytes, pData); }
        RC ReadSequence(UINT32 offset, UINT32 numBytes, vector<UINT08>* pData)
            { return m_pI2c->DoReadSequence(*this, offset, numBytes, pData); }
        RC SetSpec(I2cDevice deviceType, Tee::Priority pri);
        RC TestPort()
            { return m_pI2c->DoTestPort(*this); }
        RC Write(UINT32 offset, UINT32 data)
            { return m_pI2c->DoWrite(*this, offset, data); }
        RC WriteArray(UINT32 offset, const vector<UINT32>& data)
            { return m_pI2c->DoWriteArray(*this, offset, data); }
        RC WriteBlk(UINT32 offset, UINT32 numBytes, vector<UINT08>& data)
            { return m_pI2c->DoWriteBlk(*this, offset, numBytes, data); }
        RC WriteSequence(UINT32 offset, UINT32 numBytes, vector<UINT08>& data)
            { return m_pI2c->DoWriteSequence(*this, offset, numBytes, data); }

        void ArrayToInt(const LwU8* pArray, UINT32* pInt, UINT32 size) const;
        void IntToArray(UINT32 Int, LwU8* pArray, UINT32 size) const;

        I2c* GetI2c() { return m_pI2c; }

        SETGET_PROP(Port, UINT32);
        SETGET_PROP(Addr, UINT32);
        SETGET_PROP(OffsetLength, UINT32);
        SETGET_PROP(MessageLength, UINT32);
        SETGET_PROP(MsbFirst, bool);
        SETGET_PROP(WriteCmd, UINT08);
        SETGET_PROP(ReadCmd, UINT08);
        SETGET_PROP(VerifyReads, bool);
        SETGET_PROP(SpeedInKHz, UINT32);
        SETGET_PROP(AddrMode, UINT32);
        SETGET_PROP(Flavor, UINT32);
        SETGET_PROP_LWSTOM(PortBaseOne, UINT32);

    private:
        Dev(I2c* pI2c) : m_pI2c(pI2c) {}
        Dev(I2c* pI2c, UINT32 port, UINT32 addr): m_pI2c(pI2c), m_Port(port), m_Addr(addr) {}

        I2c*   m_pI2c          = nullptr;
        UINT32 m_Port          = 0;
        UINT32 m_Addr          = 0;
        bool   m_MsbFirst      = true; // transfer the Most significant byte first
        UINT32 m_OffsetLength  = 1;
        UINT32 m_MessageLength = 1; // this is the *default* msg length quantum
        UINT32 m_SpeedInKHz    = SPEED_100KHz;
        UINT32 m_AddrMode      = ADDRMODE_7BIT;
        UINT08 m_WriteCmd      = 0;
        UINT08 m_ReadCmd       = 0;
        bool   m_VerifyReads   = false;
        UINT32 m_Flavor        = FLAVOR_SW;
    };

    virtual ~I2c() = default;

    RC Find(vector<Dev>* pDevices)
        { return DoFind(pDevices); }
    RC GetI2cDcbDevInfo(I2cDcbDevInfo* pInfo)
        { return DoGetI2cDcbDevInfo(pInfo); }
    string GetName() const
        { return DoGetName(); }
    RC GetPortInfo(PortInfo *pInfo)
        { return DoGetPortInfo(pInfo); }
    Dev I2cDev()
        { return Dev(this); }
    Dev I2cDev(UINT32 port, UINT32 addr)
        { return Dev(this, port, addr); }
    RC Initialize(Device::LibDeviceHandle libHandle)
        { return DoInitialize(libHandle); }
    RC Initialize()
        { return DoInitialize(Device::ILWALID_LIB_HANDLE); }

    static const char* DevTypeToStr(UINT32 devType);

private:
    virtual RC DoDump(const Dev& dev, UINT32 beginOffset, UINT32 endOffset) = 0;
    virtual RC DoFind(vector<Dev>* pDevices) = 0;
    virtual RC DoGetI2cDcbDevInfo(I2cDcbDevInfo* pInfo) = 0;
    virtual string DoGetName() const = 0;
    virtual RC DoGetPortInfo(PortInfo* pInfo) = 0;
    virtual RC DoInitialize(Device::LibDeviceHandle libHandle) = 0;
    virtual RC DoPing(const Dev& dev) = 0;
    virtual RC DoRead(const Dev& dev, UINT32 offset, UINT32* pData) = 0;
    virtual RC DoReadArray(const Dev& dev, UINT32 offset, UINT32 numReads, vector<UINT32>* pData) = 0;
    virtual RC DoReadBlk(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>* pData) = 0;
    virtual RC DoReadSequence(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>* pData) = 0;
    virtual RC DoTestPort(const Dev& dev) = 0;
    virtual RC DoWrite(const Dev& dev, UINT32 offset, UINT32 data) = 0;
    virtual RC DoWriteArray(const Dev& dev, UINT32 offset, const vector<UINT32>& data) = 0;
    virtual RC DoWriteBlk(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>& data) = 0;
    virtual RC DoWriteSequence(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>& data) = 0;
};

