/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "cpusensors.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/tasker.h"
#include "core/include/xp.h"

// Reference - Intel 64 and IA-32 Arch Software Developer Manual (Volume 3B, 4)

static RC GetMsrData(UINT32 cpuid, UINT32 reg, UINT32 bitHigh, UINT32 bitLow, UINT64 *pData)
{
    RC rc;
    MASSERT(pData);

    UINT32 low, high;
    UINT64 mask, data;
    CHECK_RC(Xp::RdMsrOnCpu(cpuid, reg, &high, &low));
    data = (static_cast<UINT64>(high) << 32) | low;
    mask = (~0ULL >> (63 - bitHigh + bitLow));
    *pData = (data >> bitLow) & mask;

    return rc;
}

// Tcc - Critical Temp at which thermal control circuit is activated
RC CpuSensors::GetTccC(UINT64 *pTcc)
{
    RC rc;
    MASSERT(pTcc);

    const UINT32 MSR_TEMPERATURE_TARGET          = 0x1A2;
    const UINT32 MSR_TEMPERATURE_TARGET_TCC_HIGH = 22;
    const UINT32 MSR_TEMPERATURE_TARGET_TCC_LOW  = 16;
    static UINT64 s_Tcc = 0;

    if (!s_Tcc)
    {
        CHECK_RC(GetMsrData(0,
                            MSR_TEMPERATURE_TARGET,
                            MSR_TEMPERATURE_TARGET_TCC_HIGH,
                            MSR_TEMPERATURE_TARGET_TCC_LOW,
                            pTcc));
        s_Tcc = *pTcc;
    }
    else
    {
        *pTcc = s_Tcc;
    }

    return rc;
}

RC CpuSensors::GetTempC(UINT32 cpuid, INT64 *pTemp)
{
    RC rc;
    MASSERT(pTemp);

    const UINT32 MSR_IA32_THERM_STATUS           = 0x19C;
    const UINT32 MSR_IA32_THERM_STATUS_DRO_HIGH  = 22;
    const UINT32 MSR_IA32_THERM_STATUS_DRO_LOW   = 16;

    // DRO = digital readout
    // Temperature is represented as a negative offset from Tcc
    UINT64 dro;
    UINT64 tcc = 0;
    CHECK_RC(GetTccC(&tcc));
    CHECK_RC(GetMsrData(cpuid,
                        MSR_IA32_THERM_STATUS,
                        MSR_IA32_THERM_STATUS_DRO_HIGH,
                        MSR_IA32_THERM_STATUS_DRO_LOW,
                        &dro));
    *pTemp = static_cast<INT64>(tcc - dro);

    return rc;
}

RC CpuSensors::GetPkgTempC(UINT32 cpuid, INT64 *pTemp)
{
    RC rc;
    MASSERT(pTemp);

    const UINT32 MSR_PKG_THERMAL_STATUS          = 0x1B1;
    const UINT32 MSR_PKG_THERMAL_STATUS_PDR_HIGH = 22;
    const UINT32 MSR_PKG_THERMAL_STATUS_PDR_LOW  = 16;

    // PDR = Package digital readout
    // Pkg Temperature is represented as a negative offset from Tcc
    UINT64 pdr;
    UINT64 tcc = 0;
    CHECK_RC(GetTccC(&tcc));
    CHECK_RC(GetMsrData(cpuid,
                        MSR_PKG_THERMAL_STATUS,
                        MSR_PKG_THERMAL_STATUS_PDR_HIGH,
                        MSR_PKG_THERMAL_STATUS_PDR_LOW,
                        &pdr));
    *pTemp = static_cast<INT64>(tcc - pdr);

    return rc;
}

static RC GetEnergyUnituJ(UINT64 *pUnit)
{
    RC rc;
    MASSERT(pUnit);

    const UINT32 MSR_RAPL_POWER_UNIT                    = 0x606;
    const UINT32 MSR_RAPL_POWER_UNIT_ENERGY_STATUS_HIGH = 12;
    const UINT32 MSR_RAPL_POWER_UNIT_ENERGY_STATUS_LOW  = 8;
    static UINT64 s_EnergyUnituJ = 0;

    if (!s_EnergyUnituJ)
    {
        UINT64 data;
        CHECK_RC(GetMsrData(0,
                            MSR_RAPL_POWER_UNIT,
                            MSR_RAPL_POWER_UNIT_ENERGY_STATUS_HIGH,
                            MSR_RAPL_POWER_UNIT_ENERGY_STATUS_LOW,
                            &data));
        // Energy Status Unit = (1/2) ^ dataJ
        s_EnergyUnituJ = 1000000ULL / (1ULL << data);
        *pUnit = s_EnergyUnituJ;
    }
    else
    {
        *pUnit = s_EnergyUnituJ;
    }

    return rc;
}

RC CpuSensors::GetPkgPowermW(UINT32 cpuid, UINT64 *pPower, UINT32 pollPeriodMs)
{
    RC rc;
    MASSERT(pPower);

    UINT64 energyStart, energyEnd;

    // The lower 32 bits of the energy status register represents the
    // current value of the energy counter, updated every 1ms
    // (in increments of the energy status unit)
    // NOTE: These energy counter values are representative of all the cores in
    // the package and not based on individual cores
    const UINT32 MSR_PKG_ENERGY_STATUS             = 0x611;
    const UINT32 MSR_PKG_ENERGY_STATUS_ENERGY_HIGH = 31;
    const UINT32 MSR_PKG_ENERGY_STATUS_ENERGY_LOW  = 0;

    UINT64 energyUnituJ = 0;
    CHECK_RC(GetEnergyUnituJ(&energyUnituJ));

    CHECK_RC(GetMsrData(cpuid,
                        MSR_PKG_ENERGY_STATUS,
                        MSR_PKG_ENERGY_STATUS_ENERGY_HIGH,
                        MSR_PKG_ENERGY_STATUS_ENERGY_LOW,
                        &energyStart));
    UINT64 startTimeUs = Xp::GetWallTimeUS();
    Tasker::Sleep(pollPeriodMs);
    CHECK_RC(GetMsrData(cpuid,
                        MSR_PKG_ENERGY_STATUS,
                        MSR_PKG_ENERGY_STATUS_ENERGY_HIGH,
                        MSR_PKG_ENERGY_STATUS_ENERGY_LOW,
                        &energyEnd));
    UINT64 endTimeUs = Xp::GetWallTimeUS();

    MASSERT(startTimeUs != endTimeUs);
    MASSERT(energyEnd >= energyStart);
    *pPower = (1000ULL * (energyEnd - energyStart) * energyUnituJ) /
              (endTimeUs - startTimeUs);

    return rc;
}
