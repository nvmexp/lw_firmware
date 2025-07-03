/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   lwswitchtempmonitor.cpp
 *
 * @brief  Background logging utility.
 *
 */

#include "core/include/script.h"
#include "perlwswitchmonitor.h"
#include "bgmonitorfactory.h"
#include "device/interface/lwlink/lwlcci.h"

#include <numeric>

class LwSwitchTempMonitor : public PerLwSwitchMonitor
{
public:
    enum TempSample : UINT32
    {
        TSENSE_MAX        = (1 << 0),
        TSENSE_OFFSET_MAX = (1 << 1),
        TDIODE            = (1 << 2),
        TDIODE_OFFSET     = (1 << 3),
        TSENSE_AVG        = (1 << 4),
        TSENSE_BJT        = (1 << 5),
        CABLE_CONTROLLER  = (1 << 6),

        ALL               = (1 << 7) - 1
    };


    explicit LwSwitchTempMonitor(BgMonitorType type)
        : PerLwSwitchMonitor(type, "LwSwitch Temp")
    {
    }

    RC HandleParamsPreInit(const vector<UINT32> &Param,
                           const set<UINT32>    &MonitorDevices) override
    {
        m_SampleMask = Param[1];
        return RC::OK;
    }

    vector<SampleDesc> GetSampleDescs(TestDevicePtr pDevice) override
    {
        vector<SampleDesc> descs;

#if LWCFG(GLOBAL_LWSWITCH_IMPL_LR10) || LWCFG(GLOBAL_LWSWITCH_IMPL_LS10)
        if (pDevice->GetDeviceId() != Device::SVNP01)
        {
            if (m_SampleMask & TempSample::TSENSE_MAX)
                descs.push_back({ "TSENSE_MAX", "C", true, FLOAT });
            if (m_SampleMask & TempSample::TSENSE_OFFSET_MAX)
                descs.push_back({ "TSENSE_OFFSET_MAX", "C", true, FLOAT });
            if ((m_SampleMask & TempSample::TDIODE) && !pDevice->HasBug(2720088))
                descs.push_back({ "TDIODE", "C", true, FLOAT });
            if (m_SampleMask & TempSample::TDIODE_OFFSET)
                descs.push_back({ "TDIODE_OFFSET", "C", true, FLOAT });
            if (m_SampleMask & TempSample::TSENSE_AVG)
                descs.push_back({ "TSENSE_AVG", "C", true, FLOAT });

            if (m_SampleMask & TempSample::TSENSE_BJT)
            {
                if (pDevice->GetDeviceId() == Device::LR10)
                {
                    const UINT32 numBjts = 6;
                    for (UINT32 bjtIdx = 0; bjtIdx < numBjts; bjtIdx++)
                    {
                        string bjtStr = Utility::StrPrintf("BJT (idx = %u)", bjtIdx);
                        descs.push_back({ bjtStr, "C", true, FLOAT });
                    }
                }
                else // LS10+
                {
                    const UINT32 numBjts = 9;
                    for (UINT32 bjtIdx = 0; bjtIdx < numBjts; bjtIdx++)
                    {
                        string bjtStr = Utility::StrPrintf("TSENSE_%u", bjtIdx);
                        descs.push_back({ bjtStr, "C", true, FLOAT });
                    }
                }
            }

            auto pLwLink = pDevice->GetInterface<LwLink>();
            if (pLwLink->SupportsInterface<LwLinkCableController>())
            {
                auto pLwlCC = pLwLink->GetInterface<LwLinkCableController>();
                for (UINT32 cciId = 0; cciId < pLwlCC->GetCount(); cciId++)
                {
                    string ccTempStr;
                    const UINT64 linkMask = pLwlCC->GetLinkMask(cciId);
                    if (Utility::CountBits(linkMask) == 1)
                    {
                        ccTempStr = Utility::StrPrintf("Cable Temp (link %u)",
                                                       Utility::BitScanForward(linkMask));
                    }
                    else
                    {
                        ccTempStr = "Cable Temp (links ";
                        string comma;
                        INT32 lwrLink = Utility::BitScanForward(linkMask);
                        while (lwrLink != -1)
                        {
                            ccTempStr += comma + Utility::StrPrintf("%u", lwrLink);
                            if (comma.empty())
                                comma = ", ";
                            lwrLink = Utility::BitScanForward(linkMask, lwrLink + 1);
                        }
                        ccTempStr += ")";
                    }
                    descs.push_back({ ccTempStr, "C", true, FLOAT });
                }
            }
        }
        else
#endif
        {
            descs.push_back({ "LwSwitch Temp", "C", true, FLOAT });
        }

        return descs;
    }

    RC GatherData(TestDevicePtr pLwsDev, Sample* pSample) override
    {
        RC rc;

        vector<FLOAT32> chipTemps;
        CHECK_RC(pLwsDev->GetChipTemps(&chipTemps));

#if LWCFG(GLOBAL_LWSWITCH_IMPL_LR10) || LWCFG(GLOBAL_LWSWITCH_IMPL_LS10)
        if (pLwsDev->GetDeviceId() != Device::SVNP01)
        {
            MASSERT(chipTemps.size() >= 3);
            auto tempItr = chipTemps.begin();
            if (m_SampleMask & TempSample::TSENSE_MAX)
                pSample->Push(*tempItr);
            tempItr++;
            if (m_SampleMask & TempSample::TSENSE_OFFSET_MAX)
                pSample->Push(*tempItr);
            tempItr++;
            if ((m_SampleMask & TempSample::TDIODE) && !pLwsDev->HasBug(2720088))
                pSample->Push(*tempItr);
            tempItr++;
            if (m_SampleMask & TempSample::TDIODE_OFFSET)
                pSample->Push(*tempItr);
            tempItr++;

            if (m_SampleMask & TempSample::TSENSE_AVG)
            {
                // Callwlate average
                FLOAT32 size = static_cast<FLOAT32>(distance(tempItr, chipTemps.end()));
                FLOAT32 tsenseAvg = std::accumulate(tempItr, chipTemps.end(), 0.0) / size;
                pSample->Push(tsenseAvg);
            }

            if (m_SampleMask & TempSample::TSENSE_BJT)
            {
                for (UINT32 idx = 0; tempItr != chipTemps.end(); tempItr++, idx++)
                {
                    pSample->Push(*tempItr);
                }
            }

            if (m_SampleMask & TempSample::CABLE_CONTROLLER)
            {
                auto pLwLink = pLwsDev->GetInterface<LwLink>();
                if (!pLwLink->SupportsInterface<LwLinkCableController>())
                    return RC::OK;

                auto pLwlCC = pLwsDev->GetInterface<LwLinkCableController>();
                for (UINT32 cciId = 0; cciId < pLwlCC->GetCount(); cciId++)
                {
                    FLOAT32 lwrTemp;
                    CHECK_RC(pLwlCC->GetTempC(cciId, &lwrTemp));
                    pSample->Push(lwrTemp);
                }
            }
        }
        else
#endif
        {
            pSample->Push(chipTemps[0]);
        }

        return rc;
    }

    RC GetPrintable(const Sample& sample, vector<string>* pStrings) override
    {
        return sample.FormatAsStrings(pStrings, FLOAT, 2);
    }

private:
    UINT32 m_SampleMask = TempSample::ALL;
};

BgMonFactoryRegistrator<LwSwitchTempMonitor> RegisterLwSwTempMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::LWSWITCH_TEMP_SENSOR
);

JS_CLASS(LwSwitchTempMonitorConst);
static SObject LwSwitchTempMonitorConst_Object
(
    "LwSwitchTempMonitorConst",
    LwSwitchTempMonitorConstClass,
    0,
    0,
    "LwSwitchTempMonitorConst JS Object"
);

PROP_CONST(LwSwitchTempMonitorConst, SAMPLE_TSENSE_MAX,        static_cast<UINT32>(LwSwitchTempMonitor::TempSample::TSENSE_MAX       ));
PROP_CONST(LwSwitchTempMonitorConst, SAMPLE_TSENSE_OFFSET_MAX, static_cast<UINT32>(LwSwitchTempMonitor::TempSample::TSENSE_OFFSET_MAX));
PROP_CONST(LwSwitchTempMonitorConst, SAMPLE_TDIODE,            static_cast<UINT32>(LwSwitchTempMonitor::TempSample::TDIODE           ));
PROP_CONST(LwSwitchTempMonitorConst, SAMPLE_TSENSE_AVG,        static_cast<UINT32>(LwSwitchTempMonitor::TempSample::TSENSE_AVG       ));
PROP_CONST(LwSwitchTempMonitorConst, SAMPLE_BJT,               static_cast<UINT32>(LwSwitchTempMonitor::TempSample::TSENSE_BJT       ));
PROP_CONST(LwSwitchTempMonitorConst, SAMPLE_CABLE_CONTROLLER,  static_cast<UINT32>(LwSwitchTempMonitor::TempSample::CABLE_CONTROLLER ));
PROP_CONST(LwSwitchTempMonitorConst, SAMPLE_ALL,               static_cast<UINT32>(LwSwitchTempMonitor::TempSample::ALL              ));
