/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseq_35.c
 * @copydoc changeseq_35.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Constructor.
 * @memberof CHANGE_SEQ_35
 * @public
 * @copydetails PerfChangeSeqConstruct()
 */
FLCN_STATUS
perfChangeSeqConstruct_35
(
    CHANGE_SEQ    **ppChangeSeq,
    LwU16           size,
    LwU8            ovlIdx
)
{
    CHANGE_SEQ     *pChangeSeq   = NULL;
    CHANGE_SEQ_35  *pChangeSeq35 = NULL;
    FLCN_STATUS     status       = FLCN_OK;

    // Call super class implementation
    status = perfChangeSeqConstruct_SUPER(ppChangeSeq, size, ovlIdx);
    if (status != FLCN_OK)
    {
        goto perfChangeSeqConstruct_35_exit;
    }
    pChangeSeq   = (*ppChangeSeq);
    pChangeSeq35 = (CHANGE_SEQ_35 *)(*ppChangeSeq);

    // Set change sequence 35 specific parameters
    pChangeSeq35->super.version = LW2080_CTRL_PERF_CHANGE_SEQ_VERSION_35;

    // Allocate perf change sequence buffer for last change.
    Perf.changeSeqs.pChangeLast =
        lwosCallocType(OVL_INDEX_DMEM(perfChangeSeqClient), 1U,
                       LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE);
    if (Perf.changeSeqs.pChangeLast == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NO_FREE_MEM;
        goto perfChangeSeqConstruct_35_exit;
    }

    // Allocate perf change sequence buffer for next change.
    Perf.changeSeqs.pChangeNext =
        lwosCallocType(OVL_INDEX_DMEM(perfChangeSeqChangeNext), 1U,
                       LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE);
    if (Perf.changeSeqs.pChangeNext == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NO_FREE_MEM;
        goto perfChangeSeqConstruct_35_exit;
    }

    // Allocate perf change sequence buffer for scratch change.
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_XBAR_BOOST_FOR_MCLK))
    {
        Perf.changeSeqs.pChangeScratch =
            lwosCallocType(OVL_INDEX_DMEM(perfChangeSeqChangeNext), 1U,
                           LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE);
        if (Perf.changeSeqs.pChangeScratch == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;
            goto perfChangeSeqConstruct_35_exit;
        }
        PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(pChangeSeq, Perf.changeSeqs.pChangeScratch);
    }
    else
    {
        Perf.changeSeqs.pChangeScratch = NULL;
    }

    // Update the pointers mapping.
    pChangeSeq->pChangeForce    = &pChangeSeq35->changeForce;
    pChangeSeq->pChangeNext     = Perf.changeSeqs.pChangeNext;
    pChangeSeq->pChangeLast     = Perf.changeSeqs.pChangeLast;
    pChangeSeq->pChangeScratch  = Perf.changeSeqs.pChangeScratch;
    pChangeSeq->pChangeLwrr     = &pChangeSeq35->changeLwrr.data;
    pChangeSeq->changeSize      = sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE);

    // Set the default values for perf change sequence parameters.
    PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(pChangeSeq, Perf.changeSeqs.pChangeNext);
    PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(pChangeSeq, &pChangeSeq35->changeForce);
    PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(pChangeSeq, Perf.changeSeqs.pChangeLast);
    PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(pChangeSeq, &pChangeSeq35->changeLwrr.data);

perfChangeSeqConstruct_35_exit:
    return status;
}
