/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    cmdmgmt_sanitizer_cov.c
 * @copydoc cmdmgmt_sanitizer_cov.h
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "g_pmurpc.h"


#if SANITIZER_COV_INSTRUMENT
#include "lib_sanitizercov.h"
#include "ctrl/ctrl2080/ctrl2080ucodefuzzer.h"
#endif  // (SANITIZER_COV_INSTRUMENT)

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Retrieves the status of run-time data gathering by SanitizerCoverage
 * (GCC coverage-guided fuzzing instrumentation) such as whether it
 * is enabled or not.
 *
 * @param[out]     pParams    RPC out parameters
 *
 * @return         FLCN_OK                    on success
 * @return         FLCN_ERR_ILWALID_ARGUMENT  on invalid argument/pointer
 * @return         FLCN_ERR_NOT_SUPPORTED     if not compiled with
 *                                            SanitizerCoverage instrumentation
 */

FLCN_STATUS
pmuRpcCmdmgmtSanitizerCovGetControl
(
    RM_PMU_RPC_STRUCT_CMDMGMT_SANITIZER_COV_GET_CONTROL *pParams
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

#if SANITIZER_COV_INSTRUMENT
    SANITIZER_COV_STATUS scs;
    status = sanitizerCovGetStatus(&scs);
    pParams->used = scs.used;
    pParams->missed = scs.missed;
    pParams->bEnabled = scs.bEnabled;
#endif  // (SANITIZER_COV_INSTRUMENT)

    return status;
}

/*!
 * Adjusts the status of run-time data gathering by SanitizerCoverage
 * (GCC coverage-guided fuzzing instrumentation) such as whether it
 * is enabled or not.
 *
 * @param[in]      pParams    RPC in parameters
 *
 * @return         FLCN_OK                    on success
 * @return         FLCN_ERR_ILWALID_ARGUMENT  on invalid argument/pointer
 * @return         FLCN_ERR_NOT_SUPPORTED     if not compiled with
 *                                            SanitizerCoverage instrumentation
 */

FLCN_STATUS
pmuRpcCmdmgmtSanitizerCovSetControl
(
    RM_PMU_RPC_STRUCT_CMDMGMT_SANITIZER_COV_SET_CONTROL *pParams
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

#if SANITIZER_COV_INSTRUMENT
    SANITIZER_COV_STATUS scs;
    scs.used = pParams->used;
    scs.missed = pParams->missed;
    scs.bEnabled = pParams->bEnabled;
    status = sanitizerCovSetStatus(&scs);
#endif  // (SANITIZER_COV_INSTRUMENT)

    return status;
}

/*!
 * Retrieves run-time data gathered by SanitizerCoverage
 * (GCC coverage-guided fuzzing instrumentation).
 *
 * @param[in,out]  pParams     RPC in/out parameters
 *
 * @return         FLCN_OK                 if successful
 * @return         FLCN_ERR_OUT_OF_RANGE   if the requested number of elements
 *                                         exceeds what the RPC buffer can hold
 * @return         FLCN_ERR_NOT_SUPPORTED  if not compiled with
 *                                         SanitizerCoverage instrumentation
 * @return         FLCN_ERR_ILWALID_STATE  on internal SanitizerCoverage
 *                                         library errors
 */

FLCN_STATUS
pmuRpcCmdmgmtSanitizerCovDataGet
(
    RM_PMU_RPC_STRUCT_CMDMGMT_SANITIZER_COV_DATA_GET *pParams
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

#if SANITIZER_COV_INSTRUMENT
    if (pParams->numElements > LW2080_UCODE_FUZZER_SANITIZER_COV_RPC_MAX_ELEMENTS_PMU)
    {
        status = FLCN_ERR_OUT_OF_RANGE;
    }
    else
    {
        status = sanitizerCovCopyDataAsBytes(pParams->data,
                                             &pParams->numElements,
                                             &pParams->bDone);
    }
#endif  // (SANITIZER_COV_INSTRUMENT)

    return status;
}
