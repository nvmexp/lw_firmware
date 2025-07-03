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

#include "core/include/tee.h"
#include "device/interface/i2c.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/testdevice.h"

#define MP2888_READ_VOUT 11:0
#define MP2888_READ_IOUT 11:0
#define MP2888_READ_POUT 10:0
#define MP2888_TOTAL_LWRRENT_RES 3:3
#define INA3221_READ_VOLT 15:3
#define INA3221_READ_VOLT_SIGN 15:15

class VoltageRegulator
{
    public:
        struct PowerSample
        {
            UINT32 outVoltage_mV;
            UINT32 outLwrrent_mA;
            UINT32 outPower_mW;
        };
        struct ShuntBusVoltageSample
        {
            UINT32 chan1ShuntVolt_uV;
            UINT32 chan1BusVolt_uV;
            UINT32 chan2ShuntVolt_uV;
            UINT32 chan2BusVolt_uV;
            UINT32 chan3ShuntVolt_uV;
            UINT32 chan3BusVolt_uV;
        };
        virtual RC GetVoltageRegulatorPowerSamplerValues(PowerSample * voltReg) = 0;
        virtual RC GetVoltageRegulatorShuntBusSamplerValues(ShuntBusVoltageSample * voltReg) = 0;
        virtual RC GetName(string *name) = 0;
        unsigned int GetAddress() {return m_Addr;}
        unsigned int GetPort() {return m_Port;}
        VoltageRegulator() : m_Addr(0), m_Port(0){}
        VoltageRegulator(UINT32 port, UINT32 address, TestDevicePtr testDevice)
        {
            m_Addr = address;
            m_Port = port;
            m_pI2cDev = make_unique<I2c::Dev>(testDevice->GetInterface<I2c>()->I2cDev(m_Port, m_Addr));
            m_pI2cDev->SetOffsetLength(1);
            m_pI2cDev->SetMessageLength(2);
            m_pI2cDev->SetMsbFirst(false);
        }
        virtual ~VoltageRegulator() {}
    protected:
        unsigned int m_Addr, m_Port;
        unique_ptr<I2c::Dev> m_pI2cDev;
};

class MP2888VoltageRegulator: public VoltageRegulator
{
    public:
        RC GetVoltageRegulatorPowerSamplerValues(PowerSample * voltReg) override;
        RC GetVoltageRegulatorShuntBusSamplerValues(ShuntBusVoltageSample * voltReg) override 
        {
            return RC::UNSUPPORTED_FUNCTION;
        }
        RC GetName(string *name) override;
        MP2888VoltageRegulator(UINT32 port, UINT32 address, TestDevicePtr testDevice)
        :VoltageRegulator(port, address, testDevice)
        {
            m_TotalLwrrentRes = INT_MAX;
        }
        ~MP2888VoltageRegulator() {}
    private:
        static const UINT32 s_ReadVoutReg = 0x8B;
        static const UINT32 s_ReadIoutReg = 0x8C;
        static const UINT32 s_ReadPoutReg = 0x96;
        static const UINT32 s_TotalLwrrentResReg = 0x44;
        static const UINT32 s_LwrrentLsbMamps = 250;
        static const UINT32 s_PowerLsbMwatts = 500;
        UINT32 m_TotalLwrrentRes;
};

class INA3221VoltageRegulator: public VoltageRegulator
{
    public:
        RC GetVoltageRegulatorPowerSamplerValues(PowerSample * voltReg) override
        {
            return RC::UNSUPPORTED_FUNCTION;
        }
        RC GetVoltageRegulatorShuntBusSamplerValues(ShuntBusVoltageSample * voltReg) override;
        RC GetName(string *name) override
        {
            *name = "INA3221";
            return RC::OK;
        }
        INA3221VoltageRegulator(UINT32 port, UINT32 address, TestDevicePtr testDevice)
        :VoltageRegulator(port, address, testDevice)
        {
            m_pI2cDev->SetMsbFirst(true);
        }
        INT32 GetVoltageValue(UINT32 voltageValue, bool isShunt);
        ~INA3221VoltageRegulator() {}
    private:
        static const UINT32 s_Chan1ShuntVoltReg = 0x01;
        static const UINT32 s_Chan1BusVoltReg   = 0x02;
        static const UINT32 s_Chan2ShuntVoltReg = 0x03;
        static const UINT32 s_Chan2BusVoltReg   = 0x04;
        static const UINT32 s_Chan3ShuntVoltReg = 0x05;
        static const UINT32 s_Chan3BusVoltReg   = 0x06;
        static const UINT32 s_ShuntLsbMicroVolt = 40;
        static const UINT32 s_BusLsbMicroVolt   = 8000;
};