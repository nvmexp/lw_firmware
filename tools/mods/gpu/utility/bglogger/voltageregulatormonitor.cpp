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

/**
 * @file   voltageregulatormonitor.cpp
 *
 * @brief  Background logging utility to report output current, voltage and power via register
reads from the voltage regulators
 *
 */

#include "pergpumonitor.h"
#include "bgmonitorfactory.h"
#include "gpu/perf/voltageregulator.h"
#include "vbiosdcb4x.h"

//-----------------------------------------------------------------------------
class VoltageRegulatorMonitor : public PerGpuMonitor
{
public:
    explicit VoltageRegulatorMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "VrmPower")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;
        const UINT32 gpuInst = pSubdev->GetGpuInst();
        for (const auto& voltageRegulator : m_VoltageRegulators[gpuInst])
        {
            if (voltageRegulator.first.find("MP2888") != std::string::npos)
            {
                descs.push_back(
                {
                    voltageRegulator.first + "_VOUT", "mV", true, INT
                });
                descs.push_back(
                {
                    voltageRegulator.first + "_IOUT", "mA", true, INT
                });
                descs.push_back(
                {
                    voltageRegulator.first + "_POUT", "mW", true, INT
                });
            }
            else if (voltageRegulator.first.find("INA3221") != std::string::npos)
            {
                descs.push_back(
                {
                    voltageRegulator.first + "_Chan1ShuntVolt", "uV", true, INT
                });
                descs.push_back(
                {
                    voltageRegulator.first + "_Chan1BusVolt", "uV", true, INT
                });
                descs.push_back(
                {
                    voltageRegulator.first + "_Chan2ShuntVolt", "uV", true, INT
                });
                descs.push_back(
                {
                    voltageRegulator.first + "_Chan2BusVolt", "uV", true, INT
                });
                descs.push_back(
                {
                    voltageRegulator.first + "_Chan3ShuntVolt", "uV", true, INT
                });
                descs.push_back(
                {
                    voltageRegulator.first + "_Chan3BusVolt", "uV", true, INT
                });
            }
        }
        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        RC rc;
        I2c::I2cDcbDevInfo I2cDcbDevInfo;
        // Get all I2C devices Information
        pSubdev->GetInterface<I2c>()->GetI2cDcbDevInfo(&I2cDcbDevInfo);
        const UINT32 gpuInst = pSubdev->GetGpuInst();
        for (UINT32 dev = 0; dev < I2cDcbDevInfo.size(); ++dev)
        {
            unique_ptr<VoltageRegulator> basevoltageregulator;
            bool fetchSamples = true;
            switch (I2cDcbDevInfo[dev].Type)
            {
                case LW_DCB4X_I2C_DEVICE_TYPE_POWER_CONTRL_MP2888:
                {
                    basevoltageregulator = make_unique<MP2888VoltageRegulator>(I2cDcbDevInfo[dev].I2CLogicalPort,
                                           I2cDcbDevInfo[dev].I2CAddress, pSubdev->GetTestDevice());
                    VoltageRegulator::PowerSample vReg;
                    if (basevoltageregulator->GetVoltageRegulatorPowerSamplerValues(&vReg) != RC::OK)
                    {
                        fetchSamples = false;
                    }
                    break;
                }
                case LW_DCB4X_I2C_DEVICE_TYPE_POWER_SENSOR_INA3221:
                {
                    basevoltageregulator = make_unique<INA3221VoltageRegulator>(I2cDcbDevInfo[dev].I2CLogicalPort,
                                           I2cDcbDevInfo[dev].I2CAddress, pSubdev->GetTestDevice());
                    VoltageRegulator::ShuntBusVoltageSample vReg;
                    if (basevoltageregulator->GetVoltageRegulatorShuntBusSamplerValues(&vReg) != RC::OK)
                    {
                        fetchSamples = false;
                    }
                    break;
                }
                default:
                {
                    break;
                }
            }
            if (basevoltageregulator)
            {
                if (!fetchSamples)
                {
                    Printf(Tee::PriError, "Failed while fetching values from the voltageregulator for I2CAddress = %d, I2CLogicalPort = %d",
                                           basevoltageregulator->GetAddress(), basevoltageregulator->GetPort());
                    if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
                    {
                        Printf(Tee::PriError, "Maybe an I2CC Access Hulk License is needed?");
                    }
                    return RC::UNSUPPORTED_FUNCTION;
                }
                string name;
                CHECK_RC(basevoltageregulator->GetName(&name));
                m_VoltageRegulators[gpuInst][name] = move(basevoltageregulator);
            }
        }
        if (m_VoltageRegulators.empty())
        {
            Printf(Tee::PriError, "No voltage regulators were detected for the background logger");
            return RC::UNSUPPORTED_FUNCTION;
        }
        return RC::OK;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        RC rc;
        const UINT32 gpuInst = pSubdev->GetGpuInst();
        for (const auto& voltageRegulator : m_VoltageRegulators[gpuInst])
        {
            if (voltageRegulator.first.find("MP2888") != std::string::npos)
            {
                VoltageRegulator::PowerSample vReg;
                CHECK_RC(voltageRegulator.second->GetVoltageRegulatorPowerSamplerValues(&vReg));
                pSample->Push(vReg.outVoltage_mV);
                pSample->Push(vReg.outLwrrent_mA);
                pSample->Push(vReg.outPower_mW);
            }
            else if (voltageRegulator.first.find("INA3221") != std::string::npos)
            {
                VoltageRegulator::ShuntBusVoltageSample vReg;
                CHECK_RC(voltageRegulator.second->GetVoltageRegulatorShuntBusSamplerValues(&vReg));
                {
                    pSample->Push(vReg.chan1ShuntVolt_uV);
                    pSample->Push(vReg.chan1BusVolt_uV);
                    pSample->Push(vReg.chan2ShuntVolt_uV);
                    pSample->Push(vReg.chan2BusVolt_uV);
                    pSample->Push(vReg.chan3ShuntVolt_uV);
                    pSample->Push(vReg.chan3BusVolt_uV);
                }
            }
        }
        return RC::OK;
    }

private:
    map<UINT32, map<string, unique_ptr<VoltageRegulator>>> m_VoltageRegulators;
};

BgMonFactoryRegistrator<VoltageRegulatorMonitor> RegisterVoltageRegulatorMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::VRM_POWER
);
