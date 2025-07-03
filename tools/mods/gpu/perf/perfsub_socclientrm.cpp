/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/perf/perfsub_socclientrm.h"

#include "gpu/include/gpusbdev.h"
#include "gpu/perf/perfsub.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"

PerfSocClientRm::PerfSocClientRm(GpuSubdevice* pSubdevice, bool owned) :
    Perf20(pSubdevice, owned)
{
}

PerfSocClientRm::~PerfSocClientRm()
{
}

RC PerfSocClientRm::InnerSetPerfPoint(const PerfPoint &Setting)
{
    RC rc;
    UINT64 gpcClkHz;
    CHECK_RC(GetGpcFreqOfPerfPoint(Setting, &gpcClkHz));

    LW2080_CTRL_PERF_VF_POINT_OPS_PARAMS perfSetVfPointParams = {0};
    perfSetVfPointParams.numOfEntries = 1;
    LW2080_CTRL_PERF_VF_POINT_OPS_DOMAINS& vfDomainsParams =
            perfSetVfPointParams.vfDomains[0];
    vfDomainsParams.clkDomain = LW2080_CTRL_CLK_DOMAIN_GPC2CLK;
    vfDomainsParams.reason = LW2080_CTRL_PERF_VF_POINT_OPS_REASON_MODS_RULES;
    vfDomainsParams.operation = LW2080_CTRL_PERF_VF_POINT_OPS_OP_SET;
    vfDomainsParams.freqKHz = static_cast<LwU32>(gpcClkHz * 2U / 1000U);
    vfDomainsParams.voltuV = 0;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
                m_pSubdevice,
                LW2080_CTRL_CMD_PERF_VF_POINT_OPS,
                &perfSetVfPointParams, sizeof(perfSetVfPointParams)));
    return rc;
}

// On CheetAh this is ilwoked with -pstate_disable and -lwvdd.
// RM will not allow voltages outside of its assumed range.
// In order to make this work, we bypass RM and set the voltage directly.
// Note that with -pstate_disable RM does not touch/control voltage at all.
RC PerfSocClientRm::SetPStateDisabledVoltageMv(UINT32 voltageMv)
{
    UINT32 actual = 0;
    return CheetAh::SocPtr()->SetGpuVolt(voltageMv*1000U, &actual);
}

// On CheetAh, RM uses MODS to set/get voltage.
// Retrieve voltage straight from PMIC instead of going through RM.
// RM keeps stale voltage if we run with -pstate_disable.
RC PerfSocClientRm::GetCoreVoltageMv(UINT32 *pVoltageMv)
{
    MASSERT(pVoltageMv);
    if (!m_pSubdevice->HasFeature(Device::GPUSUB_CAN_HAVE_PSTATE20))
    {
        Printf(Tee::PriDebug, "PerfSocClientRm: "
               "Getting core voltage only supported with pstates 2.0 enabled\n");
        return OK;
    }
    RC rc;
    UINT32 uV = 0;
    rc = CheetAh::SocPtr()->GetGpuVolt(&uV);
    *pVoltageMv = uV / 1000U;
    return rc;
}

RC PerfSocClientRm::RestorePStateTable
(
    UINT32 PStateRestoreMask,
    WhichDomains d
)
{
    RC rc;
    Tasker::MutexHolder mh(GetMutex());
    LW2080_CTRL_PERF_VF_POINT_OPS_PARAMS perfSetVfPointParams = {0};
    perfSetVfPointParams.numOfEntries = 1;
    LW2080_CTRL_PERF_VF_POINT_OPS_DOMAINS& vfDomainsParams =
            perfSetVfPointParams.vfDomains[0];
    vfDomainsParams.clkDomain = LW2080_CTRL_CLK_DOMAIN_GPC2CLK;
    vfDomainsParams.reason = LW2080_CTRL_PERF_VF_POINT_OPS_REASON_MODS_RULES;
    vfDomainsParams.operation = LW2080_CTRL_PERF_VF_POINT_OPS_OP_CLEAR;
    rc = LwRmPtr()->ControlBySubdevice(
                m_pSubdevice,
                LW2080_CTRL_CMD_PERF_VF_POINT_OPS,
                &perfSetVfPointParams, sizeof(perfSetVfPointParams));
    // This is not supported before T124 - ignore
    if (rc == RC::LWRM_NOT_SUPPORTED)
        rc.Clear();
    CHECK_RC(rc);

    return Perf::RestorePStateTable(PStateRestoreMask, d);
}
