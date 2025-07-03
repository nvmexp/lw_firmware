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
 * @file   lwswitchvoltmonitor.cpp
 *
 * @brief  Background logging utility.
 *
 */

#include "perlwswitchmonitor.h"
#include "bgmonitorfactory.h"
#include "device/interface/lwlink/lwlcci.h"

class LwSwitchVoltageMonitor : public PerLwSwitchMonitor
{
public:
    explicit LwSwitchVoltageMonitor(BgMonitorType type)
        : PerLwSwitchMonitor(type, "LwSwitch Voltage")
    {
    }

    vector<SampleDesc> GetSampleDescs(TestDevicePtr pDevice) override
    {
        vector<SampleDesc> descs;
        descs.push_back({ "LwSwitch Voltage", "mV", true, INT });

        auto pLwLink = pDevice->GetInterface<LwLink>();
        if (pLwLink->SupportsInterface<LwLinkCableController>())
        {
            auto pLwlCC = pLwLink->GetInterface<LwLinkCableController>();
            for (UINT32 cciId = 0; cciId < pLwlCC->GetCount(); cciId++)
            {
                string ccVoltStr { "Cable Voltage" };
                const UINT64 linkMask = pLwlCC->GetLinkMask(cciId);
                if (Utility::CountBits(linkMask) == 1)
                {
                    ccVoltStr += Utility::StrPrintf(" (link %u)",
                                                    Utility::BitScanForward(linkMask));
                }
                else
                {
                    ccVoltStr += " (links ";
                    string comma;
                    INT32 lwrLink = Utility::BitScanForward(linkMask);
                    while (lwrLink != -1)
                    {
                        ccVoltStr += comma + Utility::StrPrintf("%u", lwrLink);
                        if (comma.empty())
                            comma = ", ";
                        lwrLink = Utility::BitScanForward(linkMask, lwrLink + 1);
                    }
                    ccVoltStr += ")";
                }
                descs.push_back({ ccVoltStr, "mV", true, INT });
            }
        }

        return descs;
    }

    RC GatherData(TestDevicePtr pLwsDev, Sample* pSample) override
    {
        UINT32 voltage = 0;
        RC rc;
        CHECK_RC(pLwsDev->GetVoltageMv(&voltage));

        pSample->Push(voltage);

        auto pLwLink = pLwsDev->GetInterface<LwLink>();
        if (pLwLink->SupportsInterface<LwLinkCableController>())
        {
            auto pLwlCC = pLwsDev->GetInterface<LwLinkCableController>();
            for (UINT32 cciId = 0; cciId < pLwlCC->GetCount(); cciId++)
            {
                UINT16 lwrVoltageMv;
                CHECK_RC(pLwlCC->GetVoltage(cciId, &lwrVoltageMv));
                pSample->Push(lwrVoltageMv);
            }
        }

        return RC::OK;
    }
};

BgMonFactoryRegistrator<LwSwitchVoltageMonitor> RegisterLwSwVoltageMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::LWSWITCH_VOLT_SENSOR
);
