/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "voltageregulator.h"

RC MP2888VoltageRegulator::GetName(string *name)
{
    RC rc;
    if (m_TotalLwrrentRes == INT_MAX)
    {
        CHECK_RC(m_pI2cDev->Read(s_TotalLwrrentResReg, &m_TotalLwrrentRes));
        m_TotalLwrrentRes = REF_VAL(MP2888_TOTAL_LWRRENT_RES, m_TotalLwrrentRes);
    }
    *name = "MP2888_HBMVDD";
    if (m_TotalLwrrentRes)
    {
        *name = "MP2888_LWVDD";
    }
    return RC::OK;
}

RC MP2888VoltageRegulator::GetVoltageRegulatorPowerSamplerValues(PowerSample * vregVal)
{
    RC rc;
    string name;
    CHECK_RC(GetName(&name));
    CHECK_RC(m_pI2cDev->Read(s_ReadVoutReg, &vregVal->outVoltage_mV));
    CHECK_RC(m_pI2cDev->Read(s_ReadIoutReg, &vregVal->outLwrrent_mA));
    CHECK_RC(m_pI2cDev->Read(s_ReadPoutReg, &vregVal->outPower_mW));
    vregVal->outVoltage_mV = REF_VAL(MP2888_READ_VOUT, vregVal->outVoltage_mV);
    vregVal->outLwrrent_mA = REF_VAL(MP2888_READ_IOUT, vregVal->outLwrrent_mA);
    vregVal->outPower_mW = REF_VAL(MP2888_READ_POUT, vregVal->outPower_mW);
    vregVal->outLwrrent_mA *= s_LwrrentLsbMamps;
    vregVal->outPower_mW *= s_PowerLsbMwatts;
    if (m_TotalLwrrentRes)
    {
        // As per the datasheet if the value of TOTAL_LWRRENT_RESOLUTION = 1,
        // then READ_IOUT and READ_POUT values need to be doubled
        vregVal->outLwrrent_mA *= 2;
        vregVal->outPower_mW *= 2;
    }
    Printf(Tee::PriLow, "Name = %s, I2cAddress = %d, I2cPort = %d, VOUT = %dmV, IOUT = %dmA, POUT = %dmW \n",
           name.c_str(), m_Addr, m_Port, vregVal->outVoltage_mV, vregVal->outLwrrent_mA, vregVal->outPower_mW);
    return RC::OK;
}

INT32 INA3221VoltageRegulator::GetVoltageValue(UINT32 voltageValue, bool isShunt = false)
{
    UINT32 result = ~(voltageValue-1);
    result = REF_VAL(INA3221_READ_VOLT, result);
    result *= isShunt ? s_ShuntLsbMicroVolt : s_BusLsbMicroVolt;
    if (REF_VAL(INA3221_READ_VOLT_SIGN, voltageValue) == 1)
    {
        return result * -1;
    }
    return static_cast<INT32>(result);
}

RC INA3221VoltageRegulator::GetVoltageRegulatorShuntBusSamplerValues(ShuntBusVoltageSample * voltReg)
{
    RC rc;
    string name;
    CHECK_RC(GetName(&name));
    CHECK_RC(m_pI2cDev->Read(s_Chan1ShuntVoltReg, &voltReg->chan1ShuntVolt_uV));
    CHECK_RC(m_pI2cDev->Read(s_Chan1BusVoltReg, &voltReg->chan1BusVolt_uV));
    CHECK_RC(m_pI2cDev->Read(s_Chan2ShuntVoltReg, &voltReg->chan2ShuntVolt_uV));
    CHECK_RC(m_pI2cDev->Read(s_Chan2BusVoltReg, &voltReg->chan2BusVolt_uV));
    CHECK_RC(m_pI2cDev->Read(s_Chan3ShuntVoltReg, &voltReg->chan3ShuntVolt_uV));
    CHECK_RC(m_pI2cDev->Read(s_Chan3BusVoltReg, &voltReg->chan3BusVolt_uV));
    voltReg->chan1ShuntVolt_uV = GetVoltageValue(voltReg->chan1ShuntVolt_uV, true);
    voltReg->chan1BusVolt_uV   = GetVoltageValue(voltReg->chan1BusVolt_uV);
    voltReg->chan2ShuntVolt_uV = GetVoltageValue(voltReg->chan2ShuntVolt_uV, true);
    voltReg->chan2BusVolt_uV   = GetVoltageValue(voltReg->chan2BusVolt_uV);
    voltReg->chan3ShuntVolt_uV = GetVoltageValue(voltReg->chan3ShuntVolt_uV, true);
    voltReg->chan3BusVolt_uV   = GetVoltageValue(voltReg->chan3BusVolt_uV);
    Printf(Tee::PriDebug, "Name = %s, I2cAddress = %d, I2cPort = %d, Chan1 Shunt = %duV, "
           "Chan1 Bus = %duV, Chan2 Shunt = %duV, Chan2 Bus = %duV, Chan3 Shunt = %duV, "
           "Chan3 Bus = %duV \n", name.c_str(), m_Addr, m_Port,
           voltReg->chan1ShuntVolt_uV, voltReg->chan1BusVolt_uV, voltReg->chan2ShuntVolt_uV,
           voltReg->chan2BusVolt_uV, voltReg->chan3ShuntVolt_uV, voltReg->chan3BusVolt_uV);
    return RC::OK;
}