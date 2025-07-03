/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_objperf.c
 * @brief  Performance-Related Interfaces
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "perf/3x/vfe.h"
#include "pmu_objperf.h"
#include "pmu_oslayer.h"
#include "pmu_objclk.h"
#include "pmgr/pwrpolicy_domgrp.h"
#include "perf/perfpolicy_pmumon.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * @copydoc OBJPERF
 */
OBJPERF  Perf;
#if (!PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
OS_TIMER PerfOsTimer;
#endif

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct the PERF object.  This includes the HAL interface as well as all
 * software objects (ex. semaphores) used by the PERF module.
 *
 * @return  FLCN_OK     On success
 * @return  Others      Error return value from failing child function
 */
FLCN_STATUS
constructPerf_IMPL(void)
{
    LwU32 limit;
    FLCN_STATUS status = FLCN_OK;

    memset(&Perf, 0x00, sizeof(OBJPERF));

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_DOMGRP))
    {
        for (limit = 0; limit < RM_PMU_PERF_DOMAIN_GROUP_LIMITS_ID_NUM; limit++)
        {
            pwrPolicyDomGrpLimitsInit(&Perf.domGrpLimits[limit]);
        }
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ELW_PSTATES_2X))
    {
        status = perfConstructPs20();
        if (status != FLCN_OK)
        {
            goto constructPerf_exit;
        }
    }

constructPerf_exit:
    return status;
}

/*!
 * PERF post-init function. Takes care of any SW initialization which
 * require running perf task such as allocating DMEM.
 */
FLCN_STATUS
perfPostInit(void)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE))
    {
        perfVfePostInit();
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ))
    {
        perfChangeSeqConstruct();
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT))
    {
        perfLimitsConstruct();
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_POLICY))
    {
        perfPoliciesConstruct();

        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_POLICY_PMUMON))
        {
            (void)perfPolicyPmumonConstruct();
        }
    }

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_SEMAPHORE))
    // Init Perf DMEM read write semaphore.
    Perf.pPerfSemaphoreRW = perfSemaphoreRWInitialize();
    if (Perf.pPerfSemaphoreRW == NULL)
    {
        status = FLCN_ERR_NO_FREE_MEM;
        PMU_HALT();
        goto perfPostInit_exit;
    }

perfPostInit_exit:
#endif

    return status;
}

/*!
 * @brief PERF RPC interface to load/unload all perf components
 *
 * @param[in]  pParams  RM_PMU_RPC_STRUCT_PERF_LOAD pointer
 */
FLCN_STATUS
pmuRpcPerfLoad
(
    RM_PMU_RPC_STRUCT_PERF_LOAD *pParams
)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_DEFINE_VFE(BASIC)
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfIlwalidation)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = perfVfeLoad(pParams->bLoad);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pmuRpcPerfLoad_exit;
        }
    }

    // TODO-JBH: Add PSTATES LOAD here

    // Load CHANGE_SEQ on supported systems
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            CHANGE_SEQ_OVERLAYS_LOAD
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            // Execute perf change sequence object interface.
            status = perfChangeSeqLoad(pParams->bLoad);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pmuRpcPerfLoad_exit;
        }
    }

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT)
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            PERF_LIMIT_OVERLAYS_BASE
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            //
            // Check that PERF_LIMITS group has been populated by RM.
            // Prevents this test code from running when RM is not using
            // PERF_LIMIT_35.
            //
            if (!boardObjGrpMaskIsZero(&(PERF_PERF_LIMITS_GET()->super.objMask)))
            {
                //
                // Do not trigger perf limit arbitration and apply here. It will
                // be trigger through ilwalidation code path after generating new
                // VF lwrve and Pstate tuple.
                // VFE LOAD -> VFE ILWALIDATION -> CALL ILWALIDATION -> PSTATE ILWALIDATION -> ARBITER ILWALIDATION
                //
                status = perfLimitsLoad(PERF_PERF_LIMITS_GET(),
                    pParams->bLoad,
                    LW_FALSE,
                    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                }
            }
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_POLICY_PMUMON)
    if (status == FLCN_OK)
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            PERF_POLICY_OVERLAYS_BOARDOBJ
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            if (pParams->bLoad)
            {
                status = perfPolicyPmumonLoad();
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                }
            }
            else
            {
                perfPolicyPmumonUnload();
            }
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_POLICY_PMUMON)
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT)

pmuRpcPerfLoad_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
