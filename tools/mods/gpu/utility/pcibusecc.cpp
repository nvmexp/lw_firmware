/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/pcibusecc.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/lwrm.h"
#include "ctrl/ctrl2080.h" // LW2080_CTRL_CMD_ECC*

RC PciBusEcc::GetDetailedEccCounts(Ecc::ErrCounts* pParams, LW2080_ECC_UNITS unit)
{
    RC rc;
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    LW2080_CTRL_ECC_GET_DETAILED_ERROR_COUNTS_PARAMS rmParams = { };

    pParams->corr   = 0;
    pParams->uncorr = 0;

    rmParams.unit = unit;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        pGpuSubdevice,
        LW2080_CTRL_CMD_ECC_GET_DETAILED_ERROR_COUNTS,
        &rmParams, sizeof(rmParams)));

    pParams->corr = rmParams.eccCounts.correctedTotalCounts;
    pParams->uncorr = rmParams.eccCounts.uncorrectedTotalCounts;

    return rc;
}

RC PciBusEcc::GetDetailedAggregateEccCounts(Ecc::ErrCounts* pParams, LW2080_ECC_UNITS unit)
{
    RC rc;
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();

    pParams->corr   = 0;
    pParams->uncorr = 0;

    LW2080_CTRL_ECC_GET_DETAILED_AGGREGATE_ERROR_COUNTS_PARAMS rmParams = { };

    rmParams.unit = unit;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        pGpuSubdevice,
        LW2080_CTRL_CMD_ECC_GET_DETAILED_AGGREGATE_ERROR_COUNTS,
        &rmParams, sizeof(rmParams)));
    MASSERT(FLD_TEST_DRF(2080, _ECC_COUNTS_FLAGS, _VALID, _TRUE, rmParams.eccCounts.flags)); //$

    pParams->corr = rmParams.eccCounts.correctedTotalCounts;
    pParams->uncorr = rmParams.eccCounts.uncorrectedTotalCounts;

    return rc;
}
