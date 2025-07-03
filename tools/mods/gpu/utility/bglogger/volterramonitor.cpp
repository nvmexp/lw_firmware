/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   volterramonitor.cpp
 *
 * @brief  Background logging utility.
 *
 */

#include "pergpumonitor.h"
#include "bgmonitorfactory.h"
#include "gpu/include/gpusbdev.h"

#include "gpu/perf/thermsub.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "core/include/utility.h"

class VolterraMonitor : public PerGpuMonitor
{
public:
    explicit VolterraMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "VolterraTemp")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;
        descs.push_back({ "VolterraTemp", "C", true, INT });
        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        Thermal *pTherm = pSubdev->GetThermal();
        VolterraData NewData;
        NewData.ReadInstance = 0;
        if ((OK != pTherm->InitializeVolterra()) ||
            (OK != pTherm->GetVolterraSlaves(&NewData.Slaves)) ||
            (NewData.Slaves.size() == 0))
        {
            Printf(Tee::PriError,
                   "VolterraMonitor : Failed to initialize on %s\n",
                   pSubdev->GpuIdentStr().c_str());
            return RC::THERMAL_SENSOR_ERROR;
        }
        const UINT32 gpuInst = pSubdev->GetGpuInst();
        m_VolterraPerGpu[gpuInst] = NewData;
        return RC::OK;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        const UINT32 gpuInst = pSubdev->GetGpuInst();

        if (m_VolterraPerGpu.find(gpuInst) == m_VolterraPerGpu.end())
        {
            return RC::OK;
        }

        Thermal*      pTherm = pSubdev->GetThermal();
        VolterraData* pData  = &m_VolterraPerGpu[gpuInst];

        for (const auto& slave : pData->Slaves)
        {
            UINT32 volterraTemp = 0;

            RC rc = pTherm->GetVolterraSlaveTemp(slave, &volterraTemp);
            if (rc == RC::THERMAL_SENSOR_ERROR)
            {
                // detected a bad read -> store the bad read for analysis later
                volterraTemp |= BAD_VALUE_BIT;
                // TODO there is no limit on how much data we collect, if the
                // sensor is dead we will go on forever!!!
                pData->BadValueInst.push_back(pData->ReadInstance);
            }
            else if (rc != RC::OK)
            {
                volterraTemp |= READ_FAIL_BIT;
                pData->FailedReadInst.push_back(pData->ReadInstance);
            }

            pData->ReadInstance++;

            pSample->Push(volterraTemp);
        }
        return RC::OK;
    }

    // Post process the times where volterra read fails. Act as a filter as
    // described  in bug 714216: zeroes are OK - just warn, but check for
    // possible bad sensors
    RC StatsChecker() override
    {
        for (const auto& dataPair : m_VolterraPerGpu)
        {
            const VolterraData *pData = &dataPair.second;
            UINT32 NumSlaves = UNSIGNED_CAST(UINT32, pData->Slaves.size());
            vector<UINT32> ErrorsPerSlave(NumSlaves);
            // tally stats for each
            for (UINT32 i = 0; i < pData->BadValueInst.size(); i++)
            {
                UINT32 SlaveId = pData->BadValueInst[i]%NumSlaves;
                ErrorsPerSlave[SlaveId]++;
            }
            for (UINT32 i = 0; i < pData->FailedReadInst.size(); i++)
            {
                UINT32 SlaveId = pData->FailedReadInst[i]%NumSlaves;
                ErrorsPerSlave[SlaveId]++;
            }
            for (UINT32 i = 0; i < ErrorsPerSlave.size(); i++)
            {
                if (0 < ErrorsPerSlave[i])
                {
                    Printf(GetStatsPri(),
                           " %d read errors on Volterra Slave%d\n",
                           ErrorsPerSlave[i], i);
                }
            }

            if (3 <= pData->BadValueInst.size())
            {

                // count errors -> 2 error conditions
                for (UINT32 i = 0; i < (pData->BadValueInst.size() - 2); i++)
                {
                    // 1: three conselwtive bad reads
                    // [39 13 38 41 | 39 R R R | 38 41 38 41 | 39 24 37 41 | ]
                    if ((1 == (pData->BadValueInst[i + 1] - pData->BadValueInst[i])) &&
                        (1 == (pData->BadValueInst[i + 2] - pData->BadValueInst[i + 1])))
                    {
                        Printf(GetStatsPri(),
                               "Found 3 conselwtive bad Volterra values!\n");
                        return RC::THERMAL_SENSOR_ERROR;
                    }
                    // 2: temporally. one slave sensor reads bad three times in a row
                    // [39 13 38 41 | R 46 46 41 | R 41 38 41 | R 24 37 41 | ]
                    UINT32 j = i + 1;
                    UINT32 slaveBadReadCount = 1;
                    while ((j < pData->BadValueInst.size()) &&
                           (slaveBadReadCount < 3) &&
                           (pData->BadValueInst[j] <= (pData->BadValueInst[i] + (NumSlaves * 2))))
                    {
                        if (((pData->BadValueInst[j] - pData->BadValueInst[i]) % NumSlaves) == 0)
                        {
                            slaveBadReadCount++;
                        }
                        j++;
                    }
                    if (slaveBadReadCount == 3)
                    {
                        Printf(GetStatsPri(),
                               "Found a Volterra slave that reports a bad value 3 times in a row\n");
                        return RC::THERMAL_SENSOR_ERROR;
                    }
                }
            }
            if (3 <= pData->FailedReadInst.size())
            {
                for (UINT32 i = 0; i < (pData->FailedReadInst.size() - 2); i++)
                {
                    // 1: three conselwtive failed reads
                    // [39 13 38 41 | 39 F F F | 38 41 38 41 | 39 24 37 41 | ]
                    if ((1 == (pData->FailedReadInst[i + 1] - pData->FailedReadInst[i])) &&
                        (1 == (pData->FailedReadInst[i + 2] - pData->FailedReadInst[i + 1])))
                    {
                        Printf(GetStatsPri(),
                               "Found 3 conselwtive failed Volterra reads!\n");
                        return RC::THERMAL_SENSOR_ERROR;
                    }
                }
            }
        }
        return OK;
    }

private:

    enum
    {
        // Volterra temperatures are always UINTs and there will never be a
        // temperature that uses bit 16, so use that bit to indicate a "bad"
        // reading
        BAD_VALUE_BIT = (1 << 16),
        READ_FAIL_BIT = (1 << 17),
    };

    // Gpu(0,0) :  VolterraTemp:[39 38 38 41 | 38 R 37 41 | 39 12 37 41 | 39 R 38 41 | ]
    // Gpu(0,0) :  VolterraTemp:[39 13 38 41 | 39 R 38 41 | 38 R 38 41 | 39 24 37 41 | ]
    // If we take the example above, the BadReadInst vector would be [5, 13, 21, 25]
    struct VolterraData
    {
        set<UINT32>    Slaves;        // slave Ids
        UINT32         ReadInstance;  // instance number for each slave read
        vector<UINT32> BadValueInst;  // store a vector of bad values to be processed
        vector<UINT32> FailedReadInst;// store a vector of failed reads to be processed
    };
    map <UINT32, VolterraData> m_VolterraPerGpu;

    static string VolterraSampleToString(const vector<UINT32>& samples)
    {
        ostringstream rtn;
        for (UINT32 i = 0; i < samples.size(); i++)
        {
            bool  bBadValue = !!(BAD_VALUE_BIT & samples[i]);
            bool  bReadFail = !!(READ_FAIL_BIT & samples[i]);
            INT32 tempVal = samples[i] & ~(BAD_VALUE_BIT | READ_FAIL_BIT);

            if (i != 0)
            {
                rtn << ' ';
            }
            if (bReadFail)
            {
                rtn << "F";
            }
            else if (BgLogger::d_Verbose || !bBadValue)
            {
                rtn << (bBadValue ? "(" : "");
                rtn << tempVal;
                rtn << (bBadValue ? ")" : "");
            }
            else
            {
                rtn << "R";
            }
        }
        return rtn.str();
    }
};

BgMonFactoryRegistrator<VolterraMonitor> RegisterVolterraMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::VOLTERRA_SENSOR
);
