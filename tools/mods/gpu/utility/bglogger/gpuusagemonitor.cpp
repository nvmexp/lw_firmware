/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "pergpumonitor.h"
#include "bgmonitorfactory.h"

class GpuUsageMonitor : public PerGpuMonitor
{
public:
    explicit GpuUsageMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "GPU Usage")
    {
    }

    RC InitPerGpu(GpuSubdevice *pSubdev) override
    {
        return RC::OK;
    }

    // Determine whether we ignore particular usage sensor
    static bool SkipLabel(UINT08 label)
    {
        switch (label)
        {
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_LABEL_DEC0:
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_LABEL_ENC0:
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_LABEL_ENC1:
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_LABEL_ENC2:
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_LABEL_DEC1:
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_LABEL_DEC2:
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_LABEL_DEC3:
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_LABEL_DEC4:
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_LABEL_JPG0:
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_LABEL_OFA:
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_LABEL_GSP:
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_LABEL_NONE:
                return true;

            default:
                break;
        }
        return false;
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;

        LW2080_CTRL_PERF_PERF_CF_SENSORS_INFO sensors = { };
        if (LwRmPtr()->ControlBySubdevice(pSubdev,
                                          LW2080_CTRL_CMD_PERF_PERF_CF_SENSORS_GET_INFO,
                                          &sensors,
                                          sizeof(sensors)) != RC::OK)
        {
            return descs;
        }

        LW2080_CTRL_PERF_PERF_CF_TOPOLOGYS_INFO info = { };
        if (LwRmPtr()->ControlBySubdevice(pSubdev,
                                          LW2080_CTRL_CMD_PERF_PERF_CF_TOPOLOGYS_GET_INFO,
                                          &info,
                                          sizeof(info)) != RC::OK)
        {
            return descs;
        }

        LW2080_CTRL_PERF_PERF_CF_TOPOLOGYS_STATUS status = { };
        status.super = info.super;
        if (LwRmPtr()->ControlBySubdevice(pSubdev,
                                          LW2080_CTRL_CMD_PERF_PERF_CF_TOPOLOGYS_GET_STATUS,
                                          &status,
                                          sizeof(status)) != RC::OK)
        {
            return descs;
        }

        const UINT32 gpuInst = pSubdev->GetGpuInst();
        MASSERT(gpuInst < 256); // Expected to be small, incrementing

        if (gpuInst >= m_SensorInfo.size())
        {
            m_SensorInfo.resize(gpuInst + 1);
        }

        GpuSubdevice::GpuSensorInfo& sensorInfo = m_SensorInfo[gpuInst];
        sensorInfo.sensors = sensors;
        sensorInfo.info    = info;
        sensorInfo.status  = status;

        UINT32 idx;
        LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(idx, &info.super.objMask.super)

            const auto& topo = info.topologys[idx];

            if (topo.bNotAvailable || SkipLabel(topo.label))
            {
                continue;
            }

            string label;
            switch (topo.label)
            {
                case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_LABEL_GR:      label = "Graphics"; break;
                case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_LABEL_FB:      label = "FB";       break;
                case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_LABEL_VID:     label = "Video";    break;
                case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_LABEL_PCIE_TX: label = "PCIE/TX";  break;
                case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_LABEL_PCIE_RX: label = "PCIE/RX";  break;

                default:
                    // Print value of any new labels added in RM that we don't handle yet
                    label = Utility::StrPrintf("0x%X", topo.label);
                    break;
            }

            if (topo.type == LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_TYPE_MIN_MAX)
            {
                label += topo.data.minMax.bMax ? " (max)" : " (min)";
            }

            string unit;
            switch (topo.unit)
            {
                case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_UNIT_PERCENTAGE:     unit = "%";    break;
                case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_UNIT_BYTES_PER_NSEC: unit = "MBps"; break;
                case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_UNIT_GHZ:            unit = "MHz";  break;

                default:
                    unit = Utility::StrPrintf("0x%X", topo.unit);
                    break;
            }

            descs.push_back({ move(label), move(unit), false, FLOAT });
        LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

        return descs;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        const UINT32 gpuInst = pSubdev->GetGpuInst();

        MASSERT(gpuInst < m_SensorInfo.size());
        GpuSubdevice::GpuSensorInfo& sensorInfo = m_SensorInfo[gpuInst];

        RC rc;
        CHECK_RC(LwRmPtr()->ControlBySubdevice(pSubdev,
                                               LW2080_CTRL_CMD_PERF_PERF_CF_TOPOLOGYS_GET_STATUS,
                                               &sensorInfo.status,
                                               sizeof(sensorInfo.status)));

        UINT32 idx;
        LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(idx, &sensorInfo.info.super.objMask.super)

            const auto& topo = sensorInfo.info.topologys[idx];
            if (topo.bNotAvailable || SkipLabel(topo.label))
            {
                continue;
            }

            float fvalue = pSubdev->GetGpuUtilizationFromSensorTopo(sensorInfo, idx);
            pSample->Push(fvalue);
        LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

        return RC::OK;
    }

    RC GetPrintable(const Sample& sample, vector<string>* pStrings) override
    {
        return sample.FormatAsStrings(pStrings, FLOAT, 2);
    }

private:
    vector<GpuSubdevice::GpuSensorInfo> m_SensorInfo;
};

BgMonFactoryRegistrator<GpuUsageMonitor> g_RegisterFactory__WhoCameUpWithThis__ThisIsNotJava___(
    BgMonitorFactories::GetInstance(),
    BgMonitorType::GPU_USAGE
);
