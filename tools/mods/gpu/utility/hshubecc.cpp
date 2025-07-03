/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/hshubecc.h"
#include "core/include/lwrm.h"
#include "ctrl/ctrl2080.h" // LW2080_CTRL_CMD_ECC*
#include "gpu/include/gpusbdev.h"

//--------------------------------------------------------------------
RC HsHubEcc::GetDetailedEccCounts
(
    Ecc::DetailedErrCounts* pParams
   ,LW2080_ECC_UNITS unit
)
{
    RC rc;
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();

    // TODO : Hopper will be adding floorsweeping of HSHUB sot that will need
    // to be accounted for here (use a different call than "Max")
    const UINT32 numHsHubs = pGpuSubdevice->GetMaxHsHubCount();
    pParams->eccCounts.clear();

    LW2080_CTRL_ECC_GET_DETAILED_ERROR_COUNTS_PARAMS rmParams = { };

    for (UINT32 hshub = 0; hshub < numHsHubs; ++hshub)
    {
        pParams->eccCounts.emplace_back(LW208F_CTRL_MMU_ECC_INJECT_ERROR_HSHUB_SUBLOCATION_COUNT);
        for (UINT32 sublocation = 0; sublocation < LW208F_CTRL_MMU_ECC_INJECT_ERROR_HSHUB_SUBLOCATION_COUNT; sublocation++)
        {
            memset(&rmParams, 0, sizeof(rmParams));
            rmParams.unit = unit;
            rmParams.location.hshub.location = hshub;
            rmParams.location.hshub.sublocation = sublocation;

            CHECK_RC(LwRmPtr()->ControlBySubdevice(
                pGpuSubdevice,
                LW2080_CTRL_CMD_ECC_GET_DETAILED_ERROR_COUNTS,
                &rmParams, sizeof(rmParams)));

            MASSERT(FLD_TEST_DRF(2080, _ECC_COUNTS_FLAGS, _VALID, _TRUE, rmParams.eccCounts.flags)); //$

            pParams->eccCounts[hshub][sublocation].corr = rmParams.eccCounts.correctedTotalCounts;
            pParams->eccCounts[hshub][sublocation].uncorr = rmParams.eccCounts.uncorrectedTotalCounts;
        }
    }
    return rc;
}

//--------------------------------------------------------------------
RC HsHubEcc::GetDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts* pParams
   ,LW2080_ECC_UNITS unit
)
{
    RC rc;
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();

    // TODO : Hopper will be adding floorsweeping of HSHUB sot that will need
    // to be accounted for here (use a different call than "Max")
    const UINT32 numHsHubs = pGpuSubdevice->GetMaxHsHubCount();
    pParams->eccCounts.clear();

    LW2080_CTRL_ECC_GET_DETAILED_AGGREGATE_ERROR_COUNTS_PARAMS rmParams = { };

    for (UINT32 hshub = 0; hshub < numHsHubs; ++hshub)
    {
        pParams->eccCounts.emplace_back(LW208F_CTRL_MMU_ECC_INJECT_ERROR_HSHUB_SUBLOCATION_COUNT);
        for (UINT32 sublocation = 0; sublocation < LW208F_CTRL_MMU_ECC_INJECT_ERROR_HSHUB_SUBLOCATION_COUNT; sublocation++)
        {
            memset(&rmParams, 0, sizeof(rmParams));
            rmParams.unit = unit;
            rmParams.location.hshub.location = hshub;
            rmParams.location.hshub.sublocation = sublocation;

            CHECK_RC(LwRmPtr()->ControlBySubdevice(
                pGpuSubdevice,
                LW2080_CTRL_CMD_ECC_GET_DETAILED_AGGREGATE_ERROR_COUNTS,
                &rmParams, sizeof(rmParams)));

            MASSERT(FLD_TEST_DRF(2080, _ECC_COUNTS_FLAGS, _VALID, _TRUE, rmParams.eccCounts.flags)); //$

            pParams->eccCounts[hshub][sublocation].corr = rmParams.eccCounts.correctedTotalCounts;
            pParams->eccCounts[hshub][sublocation].uncorr = rmParams.eccCounts.uncorrectedTotalCounts;
        }
    }
    return rc;
}

