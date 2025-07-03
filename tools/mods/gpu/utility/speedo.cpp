/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <algorithm>
#include <iterator>

#include "speedo.h"
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "gpu/perf/perfsub.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"

using Utility::ArraySize;

void SpeedoUtil::AddAltSettings(
    INT32 startBit,
    INT32 oscIdx,
    INT32 outDiv,
    INT32 mode
)
{
    if (-1 != startBit)
    {
        IsmInfo ismInfo;
        ismInfo.chainId  = UseDefault;
        ismInfo.chiplet  = UseDefault;
        ismInfo.startBit = startBit;
        ismInfo.oscIdx   = oscIdx;
        ismInfo.outDiv   = outDiv;
        ismInfo.mode     = mode;
        m_AltSettings.push_back(ismInfo);
    }
}

//-----------------------------------------------------------------------------
// Average speedos, ignoring zeros
RC SpeedoUtil::AvgSpeedos(INT32 *pAvg)
{
    vector<SpeedoValues> speedos;
    RC rc;

    CHECK_RC(ReadSpeedos(&speedos));

    UINT32 sum = 0;
    UINT32 cnt = 0;

    for (UINT32 i = 0; i < speedos.size(); i++)
    {
        for (UINT32 j = 0; j < speedos[i].values.size(); j++)
        {
            if (speedos[i].values[j] > 0) // ignore zeros
            {
                sum += speedos[i].values[j];
                cnt ++;
            }
        }
    }

    *pAvg = (cnt > 0) ? (sum / cnt) : 0;

    return OK;
}

//-----------------------------------------------------------------------------
RC SpeedoUtil::ReadSpeedos(vector<SpeedoValues> *pSpeedos)
{
    RC rc;
    MASSERT(m_pSubdev);
    const PART_TYPES Types[] = { COMP, BIN, METAL, MINI, VNSENSE, AGING, AGING2, IMON };
    vector<IsmInfo> ISMs;
    IsmInfo ism(~0, ~0, -1, m_OscIdx, m_OutDiv, m_Mode, 0, m_Adj, m_Init, m_Finit, 0, 0, m_RefClk);
    ISMs.push_back(ism);

    copy(m_AltSettings.begin(), m_AltSettings.end(), back_inserter(ISMs));

    switch (static_cast<PART_TYPES>(m_Part)) {
        case COMP:
        case BIN:
        case AGING:
        case BIN_AGING:
        case METAL:
        case MINI:
        case TSOSC:
        case VNSENSE:
        case NMEAS:
        case AGING2:
        case HOLD:
        case IMON:
        {
            SpeedoValues sv(m_Part);
            CHECK_RC(m_pSubdev->ReadSpeedometers(
                &sv.values,
                &sv.info,
                static_cast<PART_TYPES>(m_Part),
                &ISMs,
                m_Duration));
            pSpeedos->push_back(sv);
        }
        break;

        case ALL : // older GPUs read the 1st 3, newer read the last 5.
        {
            for (size_t i = 0; i < ArraySize(Types) && (rc == OK); ++i)
            {
                SpeedoValues sv(Types[i]);
                //Dont stop reading if we are missing some of the standard ISM types.
                rc = m_pSubdev->ReadSpeedometers(
                    &sv.values, &sv.info, Types[i], &ISMs, m_Duration);
                switch (rc.Get())
                {
                    case OK:    // successful read
                        pSpeedos->push_back(sv);
                        break;
                    case RC::BAD_PARAMETER:         // returned if ISM is not in the table
                    case RC::UNSUPPORTED_FUNCTION:  // returned if ISM is not supported
                        rc.Clear();
                        break;
                    default:
                        Printf(Tee::PriLow, "ReadSpeedometers, Type:%d failed with rc=%d\n",
                               Types[i], rc.Get());
                        break;
                }
            }
        }
        break;

        default: // CPM, CTRL, ISINK
        break;
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC SpeedoUtil::Calibrate()
{
    RC rc;
    MASSERT(m_pSubdev);
    Perf *pPerf = m_pSubdev->GetPerf();

    // Calibrate the speedometer to voltage colwersion
    // using a linear model (m_Speedo1 and m_Speedo2).
    pPerf->GetCoreVoltageMv((UINT32*)&m_Voltage1);
    CHECK_RC(AvgSpeedos(&m_Speedo1));

    // Since accapetable core voltages vary from card to card, we attempt to
    // change the voltage by 50 then 100. If one exists in this range, it will
    // change to it. If not, there is a problem and we bail.
    UINT32 tmpV = m_Voltage1 - 50;
    rc = pPerf->SetCoreVoltageMv(tmpV);
    CHECK_RC(pPerf->GetCoreVoltageMv((UINT32*)&m_Voltage2));
    if (m_Voltage1 == m_Voltage2)
    {
        tmpV -= 50;
        CHECK_RC(pPerf->SetCoreVoltageMv(tmpV));
        CHECK_RC(pPerf->GetCoreVoltageMv((UINT32*)&m_Voltage2));

        if (m_Voltage1 == m_Voltage2)
            return RC::VOLTAGE_OUT_OF_RANGE;
    }

    CHECK_RC(AvgSpeedos(&m_Speedo2));

    Printf(Tee::PriHigh,
           "SpeedoUtil Calibration on %s: S1@%dmv[speedo %d] "
           "S2@%dmv[speedo %d]\n",
           m_pSubdev->GpuIdentStr().c_str(),
           m_Voltage1, m_Speedo1,
           m_Voltage2, m_Speedo2);

    // Return LWVDD to original value.
    CHECK_RC(pPerf->SetCoreVoltageMv(m_Voltage1));

    m_Calibrated = true;

    return OK;
}

//-----------------------------------------------------------------------------
RC SpeedoUtil::CallwlateDroop(float *droop, UINT32 *vdd)
{
    RC rc;
    MASSERT(m_pSubdev);
    Perf *pPerf = m_pSubdev->GetPerf();

    if (!m_Calibrated)
    {
        CHECK_RC(Calibrate());
    }
    INT32 avg;
    CHECK_RC(AvgSpeedos(&avg));
    CHECK_RC(pPerf->GetCoreVoltageMv(vdd));

    // callwlate droop relative to sampled voltage
    *droop = static_cast<float>((m_Voltage1 - m_Voltage2) * (m_Speedo1 - avg))
        /((m_Speedo1 - m_Speedo2) - (m_Voltage1 - (INT32)*vdd));

    return OK;
}

