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

#include "pergpumonitor.h"
#include "bgmonitorfactory.h"
#include "core/include/lwrm.h" // for RMCTRL
#include "ctrl/ctrl2080.h"
#include "gpu/include/gpusbdev.h"

/*
 * EDPpMonitor uses the "THERMAL_MONITORS" RM API to keep track of when our
 * graphics cards see power spikes. These power spikes are referred to as
 * EDPp events by most folks at LWPU; hence the obtuse name for this monitor.
 * EDPp stands for Electrical Design Point peak (there is also EDPc for
 * continuous power).
 *
 * "THERMAL" is a giant misnomer. The RM API works by setting a 64-bit counter
 * for each monitor whenever a board level event is triggered (e.g. Droopy).
 * The counter measures for how long an event is engaged. It is incremented
 * throughout the entire MODS run and is never reset. The counters are in
 * units of UtilsClk cycles.
 *
 * On Turing boards, RM may report "EDPP_FONLY" and "EDPP_VMIN" counters.
 * "EDPP_FONLY" is also a huge misnomer. It refers to when Droopy has to lower
 * the GPU's voltage (LWVDD) in response to high current (power) events
 * (i.e. Input Current Limiting or ICL).
 * "EDPP_VMIN", an equally poor name, refers to when Droopy can no longer
 * reduce LWVDD because it has already reached Vmin. Instead, it divides
 * gpcclk by 2x, 4x, etc. Some SKUs may have "EDPP_FONLY" without "EDPP_VMIN",
 * but usually not vice-versa.
 *
 * On Volta boards, Droopy works slightly differently and gets reported in a
 * "VOLTAGE_REGULATOR" monitor. On older boards, "BLOCK_ACTIVITY" was used.
 *
 * EDPpMonitor reports its values as a percentage of time that an event
 * was engaged. There can potentially be more than one monitor of each type,
 * so the "phyInstIdx" is used to differentiate them.
 */
class EDPpMonitor : public PerGpuMonitor
{
public:
    explicit EDPpMonitor(BgMonitorType type) : PerGpuMonitor(type, "EDPP") {}

    static const char* ThermMonTypeToStr(UINT32 type)
    {
        switch (type)
        {
            case LW2080_CTRL_THERMAL_THERM_MONITOR_CLASS_VOLTAGE_REGULATOR:
                return "VOLTAGE_REGULATOR";
            case LW2080_CTRL_THERMAL_THERM_MONITOR_CLASS_BLOCK_ACTIVITY:
                return "BLOCK_ACTIVITY";
            case LW2080_CTRL_THERMAL_THERM_MONITOR_CLASS_EDPP_VMIN:
                return "EDPp_VMIN";
            case LW2080_CTRL_THERMAL_THERM_MONITOR_CLASS_EDPP_FONLY:
                return "EDPp_FONLY";
            case LW2080_CTRL_THERMAL_THERM_MONITOR_CLASS_ILWALID:
                return "INVALID";
            default:
                MASSERT(!"Unknown thermal monitor type");
                return "UNKNOWN";
        }
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;

        for (const auto& sensorData : m_SensorInfos)
        {
            const string monName = Utility::StrPrintf("%s%u",
                ThermMonTypeToStr(sensorData.second.monType),
                sensorData.second.phyInstIdx);
            descs.push_back({ monName, "%", false, FLOAT });
        }
        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        RC rc;

        LW2080_CTRL_THERM_THERM_MONITORS_INFO_PARAMS monitorInfos = {};
        CHECK_RC(LwRmPtr()->ControlBySubdevice(
            pSubdev,
            LW2080_CTRL_CMD_THERMAL_MONITORS_GET_INFO,
            &monitorInfos,
            sizeof(monitorInfos)));
        m_SensorMask = monitorInfos.super.objMask.super.pData[0];

        UINT32 ii;
        LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(ii, &monitorInfos.super.objMask.super)
            const auto& mon = monitorInfos.monitors[ii];
            m_SensorInfos[ii] = { mon.super.type, mon.phyInstIdx, 0 };
        LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

        if (m_SensorInfos.empty())
        {
            Printf(Tee::PriError, "There are no thermal monitors for -bg_edpp to use\n");
            return RC::UNSUPPORTED_FUNCTION;
        }

        Printf(Tee::PriLow, "utilsClkkHz=%u\n", monitorInfos.utilsClkFreqKhz);
        m_UtilsClkkHz = monitorInfos.utilsClkFreqKhz;

        // Initialize the counter for each EDPp monitor
        LW2080_CTRL_THERM_THERM_MONITORS_STATUS_PARAMS monitorStatuses = {};
        monitorStatuses.super.objMask.super.pData[0] = m_SensorMask;
        CHECK_RC(LwRmPtr()->ControlBySubdevice(
            pSubdev,
            LW2080_CTRL_CMD_THERMAL_MONITORS_GET_STATUS,
            &monitorStatuses,
            sizeof(monitorStatuses)));
        LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(ii, &monitorStatuses.super.objMask.super)
            m_SensorInfos[ii].lastCount = monitorStatuses.monitors[ii].counter;
        LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

        m_LastSampleTimeNs = Xp::GetWallTimeNS();

        for (const auto& sensorData : m_SensorInfos)
        {
            Printf(Tee::PriLow, "Found a thermal monitor: %s\n",
                sensorData.second.ToString().c_str());
        }

        return rc;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        RC rc;

        LW2080_CTRL_THERM_THERM_MONITORS_STATUS_PARAMS monitorStatuses = {};
        monitorStatuses.super.objMask.super.pData[0] = m_SensorMask;
        CHECK_RC(LwRmPtr()->ControlBySubdevice(
            pSubdev,
            LW2080_CTRL_CMD_THERMAL_MONITORS_GET_STATUS,
            &monitorStatuses,
            sizeof(monitorStatuses)));
        const UINT64 newTimeNs = Xp::GetWallTimeNS();

        UINT32 ii;
        LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(ii, &monitorStatuses.super.objMask.super)
            const auto elapsedTicks = monitorStatuses.monitors[ii].counter -
                                      m_SensorInfos[ii].lastCount;
            const auto timeEngagedNs = (elapsedTicks / (m_UtilsClkkHz * 1000.0)) * 1.0e9;
            FLOAT32 pct = static_cast<FLOAT32>(
                timeEngagedNs / static_cast<FLOAT64>(newTimeNs - m_LastSampleTimeNs));
            pct = min(pct, 1.0f) * 100.0f;
            m_SensorInfos[ii].lastCount = monitorStatuses.monitors[ii].counter;
            pSample->Push(pct);
        LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

        m_LastSampleTimeNs = newTimeNs;

        return rc;
    }

    RC GetPrintable(const Sample& sample, vector<string>* pStrings) override
    {
        return sample.FormatAsStrings(pStrings, FLOAT, 1);
    }

private:
    struct EDPpSensorInfo
    {
        EDPpSensorInfo() = default;
        EDPpSensorInfo(LwU8 type, UINT32 physIdx, UINT32 count) :
            monType(type), phyInstIdx(physIdx), lastCount(count) {}

        LwU8 monType = LW2080_CTRL_THERMAL_THERM_MONITOR_CLASS_ILWALID;
        LwU8 phyInstIdx = 0;
        UINT64 lastCount = 0;

        string ToString() const
        {
            return Utility::StrPrintf(
                "type=%s, phyInstIdx=%u, lastCount=%llu",
                ThermMonTypeToStr(monType), phyInstIdx, lastCount);
        }
    };

    using MonitorIdx = UINT32;
    map<MonitorIdx, EDPpSensorInfo> m_SensorInfos = {};

    UINT64 m_LastSampleTimeNs = 0;
    UINT32 m_UtilsClkkHz = 0;
    UINT32 m_SensorMask = 0;
};

static BgMonFactoryRegistrator<EDPpMonitor> RegisterEDPpMonitorFactory(
    BgMonitorFactories::GetInstance(), BgMonitorType::EDPP);
