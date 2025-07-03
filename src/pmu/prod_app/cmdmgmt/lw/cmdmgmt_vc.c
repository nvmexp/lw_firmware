/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    cmdmgmt_vc.c
 * @copydoc cmdmgmt_vc.h
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "g_pmurpc.h"

#if (PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_VC))

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
#ifdef VCAST_INSTRUMENT
extern LwBool vcastCopyData(LwU8 *, LwU32 *);
extern void   vcastResetData(void);
#endif //(VCAST_INSTRUMENT)
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/**
 * @brief           Retrieves runtime code coverage data gathered by VectorCast.
 *
 * @param[in,out]   pParams     RPC in/out parameters
 *
 * @return          FLCN_OK                 if successful
 * @return          FLCN_ERR_OUT_OF_RANGE   if code coverage data size exceeds
 *                                          the RPC buffer size
 * @return          FLCN_ERR_NOT_SUPPORTED  if VectorCast code coverage is not
 *                                          not turned on
 *
 * @ref             LW85B6_CTRL_PMU_VC_DATA_GET_PARAMS
 */
FLCN_STATUS
pmuRpcCmdmgmtVcDataGet
(
    RM_PMU_RPC_STRUCT_CMDMGMT_VC_DATA_GET *pParams
)
{
    FLCN_STATUS status = FLCN_OK;

#ifdef VCAST_INSTRUMENT
    if (pParams->bReset)
    {
        // Reset vcast buffers
        vcastResetData();
        pParams->bDone = LW_TRUE;
    }
    else
    {
        // Copy the data
        pParams->bDone = vcastCopyData((LwU8 *)pParams->dataFile, &(pParams->size));
    }

#else //(VCAST_INSTRUMENT)
    status = FLCN_ERR_NOT_SUPPORTED;

#endif //(VCAST_INSTRUMENT)

   return status;
}

#endif //(PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_VC))
