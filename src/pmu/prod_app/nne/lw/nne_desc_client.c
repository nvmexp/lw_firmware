/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   nne_desc_client.c
 * @brief  Neural Net Engine (NNE) PMU-client interface
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "task_nne.h"
#include "nne/nne_desc_client.h"
#include "objnne.h"
#include "main.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static FLCN_STATUS s_nneDescInferenceInitInternal(NNE_DESC_INFERENCE *pInference,
    LwU8 dmemOvl, LwU8 descIdx, LwU16 varInputCntStatic, LwU16 varInputCntLoop,
    LwU8 numDescOutputs, LwU8 loopCnt)
    GCC_ATTRIB_SECTION("imem_nneInferenceClient", "s_nneDescInferenceInitInternal");

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc nneDescInferenceInit
 */
FLCN_STATUS
nneDescInferenceInit
(
    NNE_DESC_INFERENCE    *pInference,
    LwU8                   dmemOvl,
    LwU8                   descIdx,
    LwU16                  varInputCntStatic,
    LwU16                  varInputCntLoop,
    LwU8                   loopCnt,
    LwrtosQueueHandle      syncQueueHandle
)
{
    NNE_DESC       *pDesc         = NULL;
    FLCN_STATUS     status        = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD_IDX(_DMEM, _ATTACH, _LS, dmemOvl)
    };

    // Sanity checking.
    if (!LWOS_DMEM_OVL_VALID(dmemOvl))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneDescInferenceInit_exit;
    }

    if (varInputCntStatic == 0 &&
        varInputCntLoop   == 0)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneDescInferenceInit_exit;
    }

    if (varInputCntStatic > LW2080_CTRL_NNE_NNE_VAR_MAX ||
        varInputCntLoop   > LW2080_CTRL_NNE_NNE_VAR_MAX)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneDescInferenceInit_exit;
    }

    if (loopCnt == 0 ||
        loopCnt > LW2080_CTRL_NNE_NNE_DESC_INFERENCE_LOOPS_MAX)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneDescInferenceInit_exit;
    }

    pDesc = NNE_DESC_GET(descIdx);
    if (pDesc == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneDescInferenceInit_exit;
    }

    if (pInference == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneDescInferenceInit_exit;
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Do the allocation
        PMU_ASSERT_OK_OR_GOTO(status,
            s_nneDescInferenceInitInternal(pInference,
                                           dmemOvl,
                                           descIdx,
                                           varInputCntStatic,
                                           varInputCntLoop,
                                           pDesc->numDescOutputs,
                                           loopCnt),
            nneDescInferenceInit_exit);

        // Allocate the return queue
        if (syncQueueHandle == NULL)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                lwrtosQueueCreate(&(pInference->syncQueueHandle),
                                  1,
                                  sizeof(NNE_DESC_INFERENCE_COMPLETE_NOTIFICATION),
                                  0),
                nneDescInferenceInit_exit);
        }
        else
        {
            pInference->syncQueueHandle = syncQueueHandle;
        }

nneDescInferenceInit_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @copydoc nneDescInferenceInitMax
 */
FLCN_STATUS
nneDescInferenceInitMax
(
    NNE_DESC_INFERENCE   *pInference,
    LwU8                  dmemOvl
)
{
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD_IDX(_DMEM, _ATTACH, _LS, dmemOvl)
    };
    FLCN_STATUS     status        = FLCN_OK;

    // Sanity checking.
    if (pInference == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneDescInferenceInitMax_exit;
    }

    if (!LWOS_DMEM_OVL_VALID(dmemOvl))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneDescInferenceInitMax_exit;
    }

    // Do the allocation.
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_nneDescInferenceInitInternal(pInference,
                                           dmemOvl,
                                           0,
                                           LW2080_CTRL_NNE_NNE_VAR_MAX,
                                           LW2080_CTRL_NNE_NNE_VAR_MAX,
                                           LW2080_CTRL_NNE_NNE_DESC_OUTPUTS_MAX,
                                           LW2080_CTRL_NNE_NNE_DESC_INFERENCE_LOOPS_MAX),
            nneDescInferenceInitMax_exit);

nneDescInferenceInitMax_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @copydoc nneDescInferenceClientEvaluate
 */
FLCN_STATUS
nneDescInferenceClientEvaluate
(
    NNE_DESC_INFERENCE   *pInference
)
{
    DISPATCH_NNE                               disp2nne;
    FLCN_STATUS                                status = FLCN_OK;
    NNE_DESC_INFERENCE_COMPLETE_NOTIFICATION   notification;

    // Populate the disp2nne command.
    disp2nne.hdr.eventType             = NNE_EVENT_ID_NNE_DESC_INFERENCE;
    disp2nne.inference.dmemOvl         = pInference->dmemOvl;
    disp2nne.inference.syncQueueHandle = pInference->syncQueueHandle;
    disp2nne.inference.pInference      = pInference;

    // Send the command to NNE.
    PMU_ASSERT_OK_OR_GOTO(status,
        lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, NNE),
                                  &disp2nne, sizeof(disp2nne)),
        nneDescInferenceClientEvaluate_exit);

    // Wait for response from NNE, all inference requests are blocking/synchronous.
    PMU_ASSERT_OK_OR_GOTO(status,
        lwrtosQueueReceive(pInference->syncQueueHandle,
                           &notification,
                           sizeof(notification),
                           lwrtosMAX_DELAY),
        nneDescInferenceClientEvaluate_exit);

    //
    // Return the status of the inference up to the client if no other
    // failure has happened.
    //
    status = notification.inferenceStatus;

nneDescInferenceClientEvaluate_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief Internal NNE_DESC_INFERENCE allocator function.
 *
 * This is a helper function that does the initializations and allocations for an
 * NNE_DESC_INFERENCE structure. It is strictly internal to nne_desc_client.c and should
 * NOT be ilwoked elsewhere as it does not:
 * - do sanity checking
 * - attach any overlays
 *
 * @param[OUT]   pInference          Allocated NNE_DESC_INFERENCE structure to initialize.
 * @param[IN]    dmemOvl             DMEM overlay that @ref pInference was allocated in.
 * @param[IN[    descIdx             Index into NNE_DESCS of the neural-net to run inference on.
 * @param[IN]    varInputCntStatic   Number of loop-independent input variables.
 * @param[IN]    varInputCntLoop     Number of loop-dependent input variables.
 * @param[IN]    loopCnt             Number of inference loops.
 * @param[IN]    syncQueueHandle     Handle of the queue that the inference completion notification
 *                                   should be sent to. If NULL, a sync queue will be allocated.
 *
 * @return FLCN_OK                     if the requested inference structure was successfully allocated.
 * @return FLCN_ERR_NO_FREE_MEM        if a sub-structure of @pInference could not be alloccated.
 */
static FLCN_STATUS
s_nneDescInferenceInitInternal
(
    NNE_DESC_INFERENCE    *pInference,
    LwU8                   dmemOvl,
    LwU8                   descIdx,
    LwU16                  varInputCntStatic,
    LwU16                  varInputCntLoop,
    LwU8                   numDescOutputs,
    LwU8                   loopCnt
)
{
    NNE_DESC_INFERENCE_LOOP   *pLoop;
    LwU8                       loopIdx;
    FLCN_STATUS                status = FLCN_OK;

    //
    // Check if the structure has be initialized already. Bail if so, this is
    // not an error.
    //
    if ((pInference->pVarInputsStatic != NULL) ||
        (pInference->pLoops           != NULL))
    {
        goto s_nneDescInferenceInitInternal_exit;
    }

    //
    // Zero out the passed in NNE_DESC_INFERENCE structure.
    // NOTE: the client should never self-initialize NNE_DESC_INFERENCE.
    //
    memset(pInference, 0, sizeof(NNE_DESC_INFERENCE));

    // Allocate the sub-structures.
    pInference->pVarInputsStatic =
        lwosCallocAligned(dmemOvl,
                          varInputCntStatic * sizeof(LW2080_CTRL_NNE_NNE_VAR_INPUT),
                          RM_PMU_FB_COPY_RW_ALIGNMENT);
    if (pInference->pVarInputsStatic == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_NO_FREE_MEM;
        goto s_nneDescInferenceInitInternal_exit;
    }

    pInference->pLoops = lwosCallocType(dmemOvl,
                                        loopCnt,
                                        NNE_DESC_INFERENCE_LOOP);
    if (pInference->pLoops == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_NO_FREE_MEM;
        goto s_nneDescInferenceInitInternal_exit;
    }

    for (loopIdx = 0; loopIdx < loopCnt; loopIdx++)
    {
        pLoop = &(pInference->pLoops[loopIdx]);
        pLoop->pVarInputs = lwosCallocAligned(dmemOvl,
                                              varInputCntLoop * sizeof(LW2080_CTRL_NNE_NNE_VAR_INPUT),
                                              RM_PMU_FB_COPY_RW_ALIGNMENT);
        if (pLoop->pVarInputs == NULL)
        {
            PMU_TRUE_BP();
            status = FLCN_ERR_NO_FREE_MEM;
            goto s_nneDescInferenceInitInternal_exit;
        }

        pLoop->pDescOutputs = lwosCallocAligned(dmemOvl,
                                                numDescOutputs * sizeof(LW2080_CTRL_NNE_NNE_DESC_OUTPUT),
                                                RM_PMU_FB_COPY_RW_ALIGNMENT);
        if (pLoop->pDescOutputs == NULL)
        {
            PMU_TRUE_BP();
            status = FLCN_ERR_NO_FREE_MEM;
            goto s_nneDescInferenceInitInternal_exit;
        }
    }

    //
    // Ensure that the new structure will not inadvertently have "valid" cache
    // data, if the client does not ilwalidate before their first inference.
    //
    LW2080_CTRL_NNE_NNE_DESC_INFERENCE_STATIC_VAR_CACHE_ILWALIDATE(
        &pInference->hdr.staticVarCache);

    // Populate fields as necessary.
    pInference->dmemOvl = dmemOvl;

s_nneDescInferenceInitInternal_exit:
    return status;
}
