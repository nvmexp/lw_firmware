/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   nne_desc.c
 * @brief  Neural Net Engine (NNE) descriptor methods
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "pmu/ssurface.h"

/* ------------------------ Application Includes ---------------------------- */
#include "task_nne.h"
#include "objnne.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "nne/nne_desc_client.h"
#include "main.h"
#include "g_pmurpc.h"
#include "nne/nne_desc_ram.h"
#include "nne/nne_parm_ram.h"
#include "nne/nne_swzl_ram.h"
#include "volt/objvolt.h"
#include "pmu_objpmgr.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjGrpIfaceModel10SetHeader              (s_nneDescIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_nneBoardObj", "s_nneDescIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry               (s_nneDescIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_nneBoardObj", "s_nneDescIfaceModel10SetEntry");
static FLCN_STATUS s_nneDescInferenceExelwte(NNE_DESC_INFERENCE *pInference)
    GCC_ATTRIB_SECTION("imem_nne", "s_nneDescInferenceExelwte");
static FLCN_STATUS s_nneDescRamStateInit(NNE_DESC_RAM_STATE *pRamState)
    GCC_ATTRIB_SECTION("imem_nne", "s_nneDescRamStateInit");
static FLCN_STATUS s_nneDescInferenceVarInputIdxGet(NNE_DESC_INFERENCE *pInference, NNE_VAR *pVar,
        NNE_DESC_INFERENCE_VAR_MAP_ENTRY **ppEntry)
    GCC_ATTRIB_SECTION("imem_nne", "s_nneDescInferenceVarInputIdxGet");
static NneDescOutputParse (s_nneDescOutputParse)
    GCC_ATTRIB_SECTION("imem_nne", "a_nneDescOutputParse");
static NneDescOutputParse (s_nneDescOutputParse_POWER_DN)
    GCC_ATTRIB_SECTION("imem_nne", "s_nneDescOutputParse_POWER_DN");
static NneDescOutputParse (s_nneDescOutputParse_POWER_TOTAL)
    GCC_ATTRIB_SECTION("imem_nne", "s_nneDescOutputParse_POWER_TOTAL");
static NneDescOutputParse (s_nneDescOutputParse_ENERGY_DN)
    GCC_ATTRIB_SECTION("imem_nne", "s_nneDescOutputParse_ENERGY_DN");
static NneDescOutputParse (s_nneDescOutputParse_ENERGY_TOTAL)
    GCC_ATTRIB_SECTION("imem_nne", "s_nneDescOutputParse_ENERGY_TOTAL");
static NneDescOutputParse (s_nneDescOutputParse_PERF)
    GCC_ATTRIB_SECTION("imem_nne", "s_nneDescOutputParse_PERF");
static FLCN_STATUS s_nneDescLoopStatusRead(
          LW2080_CTRL_NNE_NNE_DESC_INFERENCE_FP_EXCEPTION_FLAGS *pExceptions, LwU32 parmRamOffset)
    GCC_ATTRIB_SECTION("imem_nne", "s_nneDescLoopStatusRead");
static FLCN_STATUS s_nneDescsDescOutputsRead(NNE_DESC *pDesc, LW2080_CTRL_NNE_NNE_DESC_OUTPUT *pOutputs,
         LwU32 parmRamOffset)
    GCC_ATTRIB_SECTION("imem_nne", "s_nneDescsDescOutputsRead");
static FLCN_STATUS s_nneDescOutputNormalizeToOneVolt(LwBoardObjIdx voltRailIdx,
        LwU32 origVoltuV, LwU32 *pValue)
    GCC_ATTRIB_SECTION("imem_nne", "s_nneDescOutputNormalizeToOneVolt");

/*!
 * @brief   Creates the mappings from @ref NNE_VAR variables (via
 *          @ref LwBoardObjIdx) to input indices.
 *
 * @param[out]  pEntries    Array in which to add entries
 * @param[in]   numEntries  Number of entries in the entries array
 * @param[in]   pInputs     Array of inputs for which to create mappings
 * @param[in]   numInputs   The number of inputs in the pInputs array
 * @param[in]   bLoop       Whether these inputs are static or loop inputs
 *
 * @return  FLCN_OK     Success
 * @return  Others      Errors propagated from callees
 */
static FLCN_STATUS s_nneDescVarMapEntriesAdd(
        NNE_DESC_INFERENCE_VAR_MAP_ENTRY *pEntries,
        LwLength numEntries,
        LW2080_CTRL_NNE_NNE_VAR_INPUT *pInputs,
        LwLength numInputs,
        LwBool bLoop)
    GCC_ATTRIB_SECTION("imem_nne", "s_nneDescVarMapEntriesAdd");

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
//
// Number of dwords to cache in DMEM prior to writing out the values to
// descriptor RAM.
//
#define NNE_DESC_STAGING_DWORDS                                              (4)

//
// Number of swizzles to cache in DMEM prior to writing out the values to
// descriptor RAM.
//
#define NNE_DESC_SWZL_STAGING_SIZE                                           (4)

// Binary of 2^12 in IEEE-754 format
#define LW_2_POW_12_F32                                             (0x45800000)

/* ------------------------ Static Variables -------------------------------- */
static NNE_DESC_INFERENCE nneDescInferenceRpcBuf
    GCC_ATTRIB_SECTION("dmem_nneDescInferenceRpcBuf", "nneDescInferenceRpcBuf");

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc nneDescPreInit
 */
FLCN_STATUS
nneDescPreInit(void)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        nneSyncInit(&NneResident.nneSync, OVL_INDEX_OS_HEAP),
        nneDescPreInit_exit);

    nneDescInterruptInit_HAL(&Nne);

nneDescPreInit_exit:
    return status;
}

/*!
 * @copydoc nneDescPostInit
 */
FLCN_STATUS
nneDescPostInit(void)
{
    FLCN_STATUS status = FLCN_OK;

    // Create the RPC inference staging buffer
    if (PMUCFG_FEATURE_ENABLED(PMU_NNE_INFERENCE_RPC))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            nneDescInferenceInitMax(&nneDescInferenceRpcBuf,
                                    OVL_INDEX_DMEM(nneDescInferenceRpcBuf)),
            nneDescPostInit_exit);
    }

nneDescPostInit_exit:
    return status;
}

/*!
 * @brief NNE_DESCS implementation of BoardObjGrpIfaceModel10CmdHandler().
 * @memberof NNE_DESCS
 * @public
 * @copydetails BoardObjGrpIfaceModel10CmdHandler()
 */
FLCN_STATUS
nneDescBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status = FLCN_OK;

    PMU_ASSERT_OK_OR_GOTO(status,
        BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,             // _grpType
            NNE_DESC,                                   // _class
            pBuffer,                                    // _pBuffer
            s_nneDescIfaceModel10SetHeader,                   // _hdrFunc
            s_nneDescIfaceModel10SetEntry,                    // _entryFunc
            ga10xAndLater.nne.nneDescGrpSet,            // _ssElement
            OVL_INDEX_DMEM(nne),                        // _ovlIdxDmem
            BoardObjGrpVirtualTablesNotImplemented),    // _ppObjectVtables
        nneDescBoardObjGrpIfaceModel10Set_exit);

nneDescBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * @brief Constructor.
 *
 * @memberof NNE_DESC
 *
 * @protected
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
nneDescGrpIfaceModel10ObjSet_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10       *pModel10,
    BOARDOBJ         **ppBoardObj,
    LwLength           size,
    RM_PMU_BOARDOBJ   *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    BOARDOBJ          *pBoardObj;
    NNE_DESC          *pDesc;
    RM_PMU_NNE_DESC   *pTmpDesc = (RM_PMU_NNE_DESC *)pBoardObjDesc;
    LwU8               outputIdx;
    FLCN_STATUS        status;

    // Call the super-class constructor
    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, sizeof(NNE_DESC), pBoardObjDesc),
        nneDescGrpIfaceModel10ObjSet_SUPER_exit);

    pBoardObj = *(ppBoardObj);
    pDesc     = (NNE_DESC *)pBoardObj;

    // Set member variables
    pDesc->firstLayerIdx            = pTmpDesc->firstLayerIdx;
    pDesc->lastLayerIdx             = pTmpDesc->lastLayerIdx;
    pDesc->firstVarIdx              = pTmpDesc->firstVarIdx;
    pDesc->lastVarIdx               = pTmpDesc->lastVarIdx;
    pDesc->numLayers                = pTmpDesc->numLayers;
    pDesc->numDescOutputs           = pTmpDesc->numOutputs;
    pDesc->maxInterLayerNumOutputs  = pTmpDesc->maxInterLayerNumOutputs;
    pDesc->networkVersion           = pTmpDesc->networkVersion;

    // Initialize the RAM state
    PMU_ASSERT_OK_OR_GOTO(status,
        s_nneDescRamStateInit(&(pDesc->ramState)),
        nneDescGrpIfaceModel10ObjSet_SUPER_exit);

    // Init the DESC_OUTPUTS array
    if (pDesc->numDescOutputs > 0)
    {
        // Only allocate outputs array if it hasn't already been already
        if (pDesc->pDescOutputs == NULL)
        {
            pDesc->pDescOutputs =
                lwosCallocType(pBObjGrp->ovlIdxDmem, pDesc->numDescOutputs,
                               LW2080_CTRL_NNE_NNE_DESC_OUTPUT_ID);
        }

        for (outputIdx = 0; outputIdx < pDesc->numDescOutputs; outputIdx++)
        {
            pDesc->pDescOutputs[outputIdx] = pTmpDesc->outputs[outputIdx];
        }
    }

    // Copy the inputNormInfo
    pDesc->inputNormInfo = pTmpDesc->inputNormInfo;

nneDescGrpIfaceModel10ObjSet_SUPER_exit:
    return status;
}

/*!
 * @copydoc nneDescInferenceClientHandler
 */
FLCN_STATUS
nneDescInferenceClientHandler
(
    DISPATCH_NNE_NNE_DESC_INFERENCE *pDispatch
)
{
    NNE_DESC_INFERENCE                        *pInference    = pDispatch->pInference;
    FLCN_STATUS                                status        = FLCN_OK;
    LwU8                                       dmemOvl       = pDispatch->dmemOvl;
    NNE_DESC_INFERENCE_COMPLETE_NOTIFICATION   notification;
    OSTASK_OVL_DESC                            ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD_IDX(_DMEM, _ATTACH, _LS, dmemOvl)
        OSTASK_OVL_DESC_DEFINE_NNE_INFERENCE
    };

    // Sanity checking.
    if (!LWOS_DMEM_OVL_VALID(dmemOvl))
    {
        PMU_TRUE_BP();
        notification.inferenceStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneDescInferenceClientHandler_exit;
    }

    // Run inference.
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        notification.inferenceStatus = s_nneDescInferenceExelwte(pInference);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

nneDescInferenceClientHandler_exit:
    // Send the inference completion confirmation message back to the requester.
    status = lwrtosQueueSendBlocking(pInference->syncQueueHandle,
                                     &notification,
                                     sizeof(notification));
    if (status != FLCN_OK)
    {
        PMU_TRUE_BP();
    }

    return status;
}

/*!
 * @copydoc nneDescDescIsLoaded
 */
LwBool
nneDescsDescIsLoaded
(
    NNE_DESCS *pDescs,
    NNE_DESC  *pDesc
)
{
    if ((pDescs == NULL) ||
        (pDesc == NULL))
    {
        PMU_BREAKPOINT(); // NJ??
        return LW_FALSE;
    }
    else
    {
        return pDescs->ramState.seqId == pDesc->ramState.seqId;
    }
}

/*!
 * @copydoc nneDescsDescLoad
 */
FLCN_STATUS
nneDescsDescLoad
(
    NNE_DESCS   *pDescs,
    NNE_DESC    *pDesc
)
{
    NNE_LAYER     *pLwrLayer;
    LwU32         descStaging[NNE_DESC_STAGING_DWORDS];
    LwU32         lwrDescRamOffset;
    LwU32         layerCnt        = 0;
    LwU32         numLayers;
    LwU32         numInputs;
    LwU32         numSwzl;
    LwU16         lwrLayerIdx;
    LwU8          descHeaderSize;
    LwU8          layerEntrySize;
    LwU8          descStagingSize = NNE_DESC_STAGING_DWORDS * sizeof(LwU32);
    LwU8          bufIdx;
    LwU8          numBias;
    FLCN_STATUS   status          = FLCN_OK;

    // Sanity checking.
    if ((pDescs == NULL) ||
        (pDesc  == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneDescsDescLoad_exit;
    }

    //
    // Early return if the descriptor has already been loaded. A loaded
    // descriptor is assumed to have all of its necessary data loaded into
    // the DLC RAM's and, with the except of programming its inputs/outputs,
    // is ready for inference.
    //
    if (nneDescsDescIsLoaded(pDescs, pDesc))
    {
        goto nneDescsDescLoad_exit;
    }

    // Free all previous allocations.
    PMU_ASSERT_OK_OR_GOTO(status,
        nneSwzlRamReset(),
        nneDescsDescLoad_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        nneParmRamReset(),
        nneDescsDescLoad_exit);

    //
    // Allocate the front and back buffers to store all the intermediate results
    // when running inference on this neural-net.
    //
    for (bufIdx = 0; bufIdx < NNE_DESC_BUFFER_NUM_BUFFERS; bufIdx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            nneParmRamAlloc(pDesc->maxInterLayerNumOutputs * sizeof(LwU32),
                            &(pDesc->ramState.buffers[bufIdx].parmRamOffset)),
            nneDescsDescLoad_exit);
    }

    // Allocate space in the descriptor RAM for this layer's data.
    descHeaderSize   = nneDescRamNetworkHeaderSizeGet_HAL();
    layerEntrySize   = nneDescRamLayerEntrySizeGet_HAL();

    PMU_ASSERT_OK_OR_GOTO(status,
        nneDescNumLayersGet(pDesc, &numLayers),
        nneDescsDescLoad_exit);

    status = nneDescRamAlloc(descHeaderSize + (numLayers * layerEntrySize),
                             &(pDesc->ramState.descRamOffset),
                             &(pDesc->ramState.nnetId));
    lwrDescRamOffset = pDesc->ramState.descRamOffset; // NJ??
    if (status != FLCN_OK)
    {
        PMU_TRUE_BP();
        goto nneDescsDescLoad_exit;
    }

    //
    // Get the binary representation of this descriptor, that the DLC understands.
    // Set the paramRamStatusOffset to 0 during load-time with the expectation
    // of being overwritten when allocating resources for an inference.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        nneDescNetworkHeaderBinaryFormatGet_HAL(&Nne,
                                                pDesc,
                                                descStaging,
                                                descStagingSize,
                                                0,  // loopMax
                                                0), // paramRamStatusOffset
        nneDescsDescLoad_exit);

    // Write out the NNE_DESC header to descriptor RAM.
    PMU_ASSERT_OK_OR_GOTO(status,
        nneDescRamAccess_HAL(&Nne, descStaging, descHeaderSize, lwrDescRamOffset, LW_TRUE),
        nneDescsDescLoad_exit);

    lwrDescRamOffset += descHeaderSize;

    // Set the new sequence ID
    pDescs->ramState.seqId++;
    pDesc->ramState.seqId = pDescs->ramState.seqId;

    lwrLayerIdx = pDesc->firstLayerIdx;
    while (lwrLayerIdx != LW2080_CTRL_NNE_NNE_LAYER_INDEX_ILWALID)
    {
        // Get the layer.
        pLwrLayer = NNE_LAYER_GET(lwrLayerIdx);
        if (pLwrLayer == NULL)
        {
            PMU_TRUE_BP();
            status = FLCN_ERR_ILWALID_STATE;
            goto nneDescsDescLoad_exit;
        }

        //
        // If not the first layer, allocate and write the swizzle for the layer.
        // The first layer's swizzle must be allocated and written at inference-time
        // as the numebr of loop-dependent variables and number inference loops
        // is not known at load-time.
        //
        if (layerCnt > 0)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                nneLayerNumInputsGet(pLwrLayer, &numInputs),
                nneDescsDescLoad_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                nneLayerNumBiasGet(pLwrLayer, &numBias),
                nneDescsDescLoad_exit);

            numSwzl = numInputs + numBias;

            PMU_ASSERT_OK_OR_GOTO(status,
                nneSwzlRamAlloc(numSwzl * sizeof(LwU32),
                                &(pLwrLayer->ramState.swzlRamOffset)),
                nneDescsDescLoad_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                nneSwzlDirectMappingWrite(pLwrLayer->ramState.swzlRamOffset,
                                          pDesc->ramState.buffers[layerCnt % 2].parmRamOffset,
                                          numInputs),
                nneDescsDescLoad_exit);

            if (numBias > 0)
            {
                PMU_ASSERT_OK_OR_GOTO(status,
                    nneSwzlDirectMappingWrite(pLwrLayer->ramState.swzlRamOffset + (numInputs * sizeof(LwU32)),
                                              NNE_PARM_RAM_1_CONST_OFFSET,
                                              numBias),
                    nneDescsDescLoad_exit);
            }
        }

        //
        // Configure the layer to write it's outputs to one of the front/back buffers,
        // alternating between layers.
        //
        pLwrLayer->ramState.parmRamOutputOffset = pDesc->ramState.buffers[(++layerCnt) % 2].parmRamOffset;

        // Load the layer.
        PMU_ASSERT_OK_OR_GOTO(status,
            nneLayersLayerLoad(NNE_LAYERS_GET(), pLwrLayer),
            nneDescsDescLoad_exit);

        //
        // Write the layer's metadata into the descriptor RAM staging buffer.
        // Set the swzlRamOffset and paramRamOutputOffset to 0 during load-time with the
        // expectation of being overwritten when allocating resources for an inference.
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            nneDescLayerEntryBinaryFormatGet_HAL(&Nne,
                                                 pLwrLayer,
                                                 descStaging,
                                                 descStagingSize,
                                                 pLwrLayer->ramState.swzlRamOffset,
                                                 pLwrLayer->ramState.parmRamOutputOffset,
                                                 (pDesc->lastLayerIdx == lwrLayerIdx)),
            nneDescsDescLoad_exit);

        // Write the layer to descriptor RAM.
        PMU_ASSERT_OK_OR_GOTO(status,
            nneDescRamAccess_HAL(&Nne, descStaging, lwrDescRamOffset, layerEntrySize, LW_TRUE),
            nneDescsDescLoad_exit);

        lwrDescRamOffset += layerEntrySize;

        // Move our pointer to the next layer.
        lwrLayerIdx = pLwrLayer->nextLayerIdx;
    }

nneDescsDescLoad_exit:
    return status;
}

/*!
 * @brief   Runs inference on a given NNE_DESC.
 */
FLCN_STATUS
pmuRpcNneNneDescInference
(
    RM_PMU_RPC_STRUCT_NNE_NNE_DESC_INFERENCE *pParams
)
{
    OSTASK_OVL_DESC   ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, nneDescInferenceRpcBuf)
        OSTASK_OVL_DESC_DEFINE_NNE_INFERENCE
    };
    LwU8              loopIdx;
    LwU8              loopCnt;
    FLCN_STATUS       status        = FLCN_OK;

    //
    // NNE inference RPC requires ODP because it stages all inference inputs/outputs
    // in the nneDescInferenceRpcBuf DMEM overlay, which at ~80KB, is larger than the
    // size of all of current PMU DMEM cache. Therefore, it is not possible to have entire
    // overlay in DMEM at once, therefore necessitating ODP.
    //
#ifndef ON_DEMAND_PAGING_BLK
    ct_assert(!PMUCFG_FEATURE_ENABLED(PMU_NNE_INFERENCE_RPC));
#endif

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        //
        // The cache is unsupported for the RPC path, so ensure that it is
        // ilwalidated.
        //
        LW2080_CTRL_NNE_NNE_DESC_INFERENCE_STATIC_VAR_CACHE_ILWALIDATE(
            &pParams->inferenceHdr.staticVarCache);

        // Copy all data to the RPC inference staging buffer.
        nneDescInferenceRpcBuf.hdr = pParams->inferenceHdr;
        loopCnt = nneDescInferenceRpcBuf.hdr.loopCnt;

        if ((LW2080_CTRL_NNE_NNE_VAR_MAX < nneDescInferenceRpcBuf.hdr.varInputCntStatic) ||
            (LW2080_CTRL_NNE_NNE_VAR_MAX < nneDescInferenceRpcBuf.hdr.varInputCntLoop)   ||
            (LW2080_CTRL_NNE_NNE_DESC_INFERENCE_LOOPS_MAX < nneDescInferenceRpcBuf.hdr.loopCnt))
        {
            PMU_TRUE_BP();
            status = FLCN_ERR_NOT_SUPPORTED;
            goto pmuRpcNneNneDescInference_exit;
        }

        PMU_ASSERT_OK_OR_GOTO(status,
            ssurfaceRd(nneDescInferenceRpcBuf.pVarInputsStatic,
                    RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.nne.nneDescInference.varInputsStaticAligned),
                    nneDescInferenceRpcBuf.hdr.varInputCntStatic * sizeof(LW2080_CTRL_NNE_NNE_VAR_INPUT)),
            pmuRpcNneNneDescInference_exit);

        // Normalize inputs before exelwting.
        PMU_ASSERT_OK_OR_GOTO(status,
            nneVarsVarInputsNormalize(
                nneDescInferenceRpcBuf.pVarInputsStatic,
                nneDescInferenceRpcBuf.hdr.varInputCntStatic,
                nneDescInferenceRpcBuf.hdr.descIdx,
                &(nneDescInferenceRpcBuf.hdr.inputNormStatus),
                &(LwF32){0}),
            pmuRpcNneNneDescInference_exit);

        for (loopIdx = 0; loopIdx < loopCnt; loopIdx++)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                ssurfaceRd(nneDescInferenceRpcBuf.pLoops[loopIdx].pVarInputs,
                        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.nne.nneDescInference.loops[loopIdx].varInputsAligned),
                        nneDescInferenceRpcBuf.hdr.varInputCntLoop * sizeof(LW2080_CTRL_NNE_NNE_VAR_INPUT)),
                pmuRpcNneNneDescInference_exit);
        }

        // Run inference.
        pParams->inferenceStatus = s_nneDescInferenceExelwte(&nneDescInferenceRpcBuf);

        // Copy out the number of outputs and the profiling data.
        pParams->inferenceHdr.descOutputCnt = nneDescInferenceRpcBuf.hdr.descOutputCnt;
        pParams->inferenceHdr.profiling = nneDescInferenceRpcBuf.hdr.profiling;

        // Copy out all outputs.
        for (loopIdx = 0; loopIdx < loopCnt; loopIdx++)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                ssurfaceWr(&nneDescInferenceRpcBuf.pLoops[loopIdx].exceptions,
                    RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.nne.nneDescInference.loops[loopIdx].exceptions),
                    sizeof(nneDescInferenceRpcBuf.pLoops[loopIdx].exceptions)),
            pmuRpcNneNneDescInference_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                ssurfaceWr(nneDescInferenceRpcBuf.pLoops[loopIdx].pDescOutputs,
                    RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.nne.nneDescInference.loops[loopIdx].descOutputsAligned),
                    pParams->inferenceHdr.descOutputCnt * sizeof(LW2080_CTRL_NNE_NNE_DESC_OUTPUT)),
                pmuRpcNneNneDescInference_exit);
        }

pmuRpcNneNneDescInference_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @copydoc nneDescNumLayersGet
 */
FLCN_STATUS
nneDescNumLayersGet
(
    NNE_DESC   *pDesc,
    LwU32      *pNumLayers
)
{
    FLCN_STATUS status = FLCN_OK;

    if ((pDesc      == NULL) ||
        (pNumLayers == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneDescNumLayersGet_exit;
    }

    *pNumLayers = pDesc->numLayers;

nneDescNumLayersGet_exit:
    return status;
}

/*!
 * @brief Get the global sequence ID.
 *
 * The global sequence ID is the sequence ID of the lwrrently loaded NNE_DESC.
 *
 * @param[IN] pDescs   Pointer to the global NNE_DESCS.
 *
 * @return The global sequence ID.
 */
FLCN_STATUS
nneDescsSeqIdGet
(
    NNE_DESCS   *pDescs,
    LwU32       *pSeqId
)
{
    FLCN_STATUS status = FLCN_OK;

    if ((pDescs == NULL) || 
        (pSeqId == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneDescsSeqIdGet_exit;
    }

    *pSeqId = pDescs->ramState.seqId;

nneDescsSeqIdGet_exit:
    return status;
}

/*!
 * @copydoc nneDescInferenceConfig
 */
FLCN_STATUS
nneDescInferenceConfig
(
    NNE_DESC   *pDesc,
    LwU8        loopCnt,
    LwU32       swzlRamOffset,
    LwU32       parmRamOutputOffset,
    LwU32       parmRamStatusOffset
)
{
    LwU32         descStaging[NNE_DESC_STAGING_DWORDS];
    LwU32         descHeaderSize;
    LwU32         descLayerEntrySize;
    LwU32         descRamLayerOffset;
    NNE_LAYER     *pLayer;
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    if (pDesc == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneDescInferenceConfig_exit;
    }

    // Set the number of descriptors loaded to 1.
    PMU_ASSERT_OK_OR_GOTO(status,
        nneDescRamNumDescLoadedSet_HAL(&Nne, 1),
        nneDescInferenceConfig_exit);

    //
    // Since we are always assuming only one descriptor is loaded, sanity check
    // that the passed in NNE_DESC descriptor RAM data is loaded at the location
    // where the DLC reads the first neural-net (i.e. nnetId == 0). This is immediately
    // after the global DLC header in the descriptor RAM.
    //
    if (pDesc->ramState.descRamOffset != nneDescRamHeaderSizeGet_HAL())
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_STATE;
        goto nneDescInferenceConfig_exit;
    }

    // Program the descriptor header.
    PMU_ASSERT_OK_OR_GOTO(status,
        nneDescNetworkHeaderBinaryFormatGet_HAL(&Nne,
                                                pDesc,
                                                descStaging,
                                                NNE_DESC_STAGING_DWORDS * sizeof(LwU32),
                                                loopCnt,
                                                parmRamStatusOffset),
        nneDescInferenceConfig_exit);

    descHeaderSize = nneDescRamNetworkHeaderSizeGet_HAL();

    PMU_ASSERT_OK_OR_GOTO(status,
        nneDescRamAccess_HAL(&Nne,
                             descStaging,
                             pDesc->ramState.descRamOffset,
                             descHeaderSize,
                             LW_TRUE),
        nneDescInferenceConfig_exit);

    // Program the input layer.
    pLayer = NNE_LAYER_GET(pDesc->firstLayerIdx);
    if (pLayer == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_STATE;
        goto nneDescInferenceConfig_exit;
    }

    descRamLayerOffset             = pDesc->ramState.descRamOffset + descHeaderSize;
    descLayerEntrySize             = nneDescRamLayerEntrySizeGet_HAL();
    pLayer->ramState.swzlRamOffset = swzlRamOffset;

    PMU_ASSERT_OK_OR_GOTO(status,
        nneDescLayerEntryBinaryFormatGet_HAL(&Nne,
                                             pLayer,
                                             descStaging,
                                             descLayerEntrySize,
                                             pLayer->ramState.swzlRamOffset,
                                             pLayer->ramState.parmRamOutputOffset,
                                             LW_FALSE),
        nneDescInferenceConfig_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        nneDescRamAccess_HAL(&Nne,
                             descStaging,
                             descRamLayerOffset,
                             descLayerEntrySize,
                             LW_TRUE),
        nneDescInferenceConfig_exit);

    // Program the output layer.
    pLayer = NNE_LAYER_GET(pDesc->lastLayerIdx);
    if (pLayer == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_STATE;
        goto nneDescInferenceConfig_exit;
    }

    pLayer->ramState.parmRamOutputOffset = parmRamOutputOffset;

    PMU_ASSERT_OK_OR_GOTO(status,
        nneDescLayerEntryBinaryFormatGet_HAL(&Nne,
                                             pLayer,
                                             descStaging,
                                             descLayerEntrySize,
                                             pLayer->ramState.swzlRamOffset,
                                             pLayer->ramState.parmRamOutputOffset,
                                             LW_TRUE),
        nneDescInferenceConfig_exit);

    descRamLayerOffset = pDesc->ramState.descRamOffset + descHeaderSize +
                         ((pDesc->numLayers - 1) * descLayerEntrySize);

    PMU_ASSERT_OK_OR_GOTO(status,
        nneDescRamAccess_HAL(&Nne,
                             descStaging,
                             descRamLayerOffset,
                             descLayerEntrySize,
                             LW_TRUE),
        nneDescInferenceConfig_exit);

nneDescInferenceConfig_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10SetHeader()
 *
 * @memberof NNE_DESCS
 *
 * @private
 */
static FLCN_STATUS
s_nneDescIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    NNE_DESCS                                        *pDescs;
    const RM_PMU_NNE_NNE_DESC_BOARDOBJGRP_SET_HEADER *pSet =
        (const RM_PMU_NNE_NNE_DESC_BOARDOBJGRP_SET_HEADER *)pHdrDesc;
    FLCN_STATUS                                       status;

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc),
        s_nneDescIfaceModel10SetHeader_exit);

    pDescs = (NNE_DESCS *)pBObjGrp;

    // Set member variables
    pDescs->numVarIdx      = pSet->numVarIdx;
    pDescs->ramState.seqId = 0;
    if (pDescs->pVarIdxArray == NULL)
    {
        pDescs->pVarIdxArray = lwosCalloc(pDescs->super.super.ovlIdxDmem,
                                          pSet->numVarIdx * sizeof(*pDescs->pVarIdxArray));
    }
    memcpy(pDescs->pVarIdxArray, pSet->varIdxArray, pSet->numVarIdx * sizeof(*pDescs->pVarIdxArray));

s_nneDescIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry()
 *
 * @memberof NNE_DESCS
 *
 * @private
 */
static FLCN_STATUS
s_nneDescIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_NNE_NNE_DESC_TYPE_FC_10:
        {
            status = nneDescGrpIfaceModel10ObjSet_FC_10(pModel10, ppBoardObj,
                                            sizeof(NNE_DESC_FC_10), pBuf);
            break;
        }
        default:
        {
            PMU_TRUE_BP();
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
        }
    }

    return status;
}

/*!
 * @brief NNE-side exelwtion of all inference requests.
 *
 * @param[IN/OUT]   pInference   structure holding all inputs and outputs for an inference requests
 *
 * @return FLCN_OK   Inference was successfully handled, this does not imply that all inference loops yielded valid results
 *
 * @memberof NNE_DESC
 *
 * @public
 */
static FLCN_STATUS
s_nneDescInferenceExelwte
(
    NNE_DESC_INFERENCE   *pInference
)
{
    NNE_SWZL                           swzlStaging[NNE_DESC_SWZL_STAGING_SIZE];
    NNE_DESCS                         *pDescs = NNE_DESCS_GET();
    NNE_DESC                          *pDesc;
    NNE_LAYER                         *pLayer;
    NNE_VAR                           *pVar;
    LwU32                              parmRamLoopInputOffset;
    LwU32                              parmRamStatusOffset;
    LwU32                              parmRamOutputOffset;
    LwU32                              swzlRamOffset;
    LwU32                              numDescInputs;
    LwU32                              numSwzl;
    LwU32                              varInputsLoopSize;
    LwU32                              swzlIdx;
    LwU32                              swzlStagingIdx;
    LwU8                               loopIdx;
    LwU8                               numBias;
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if (pInference == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_nneDescInferenceExelwte_exit;
    }

    // Initialize all profiling data.
    memset(pInference->hdr.profiling.elapsedTimes,
           0,
           sizeof(LwU64_ALIGN32) * LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PROFILING_REGION_MAX);

    // Start profiling total time to run inference, if profiling is enabled.
    if (PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC_INFERENCE_PROFILING))
    {
        NNE_DESC_INFERENCE_PROFILE_BEGIN(
            &(pInference->hdr.profiling.elapsedTimes[LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PROFILING_REGION_TOTAL_TIME]));
    }

    pDesc = NNE_DESC_GET(pInference->hdr.descIdx);
    if (pDesc == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_nneDescInferenceExelwte_exit;
    }

    if (pInference->hdr.loopCnt == 0)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_nneDescInferenceExelwte_exit;
    }

    if ((pInference->hdr.loopCnt > 1) &&
        (pInference->hdr.varInputCntLoop == 0))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_nneDescInferenceExelwte_exit;
    }

    numDescInputs = pDesc->lastVarIdx - pDesc->firstVarIdx + 1;
    if (numDescInputs > (pInference->hdr.varInputCntStatic + pInference->hdr.varInputCntLoop))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_nneDescInferenceExelwte_exit;
    }

    // Ensure that the inference's variable map is ilwalidated before starting.
    PMU_ASSERT_OK_OR_GOTO(status,
        nneDescInferenceVarMapIlwalidate(pInference),
        s_nneDescInferenceExelwte_exit);

    // Start profiling time needed to load an NNE_DESC, if profiling is enabled.
    if (PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC_INFERENCE_PROFILING))
    {
        pInference->hdr.profiling.bDescriptorLoaded =
            nneDescsDescIsLoaded(NNE_DESCS_GET(), pDesc);

        NNE_DESC_INFERENCE_PROFILE_BEGIN(
            &(pInference->hdr.profiling.elapsedTimes[LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PROFILING_REGION_DESC_LOAD]));
    }

    // Load the neural net descriptor.
    PMU_ASSERT_OK_OR_GOTO(status,
        nneDescsDescLoad(NNE_DESCS_GET(), pDesc),
        s_nneDescInferenceExelwte_exit);

    //
    // End profiling time needed to load an NNE_DESC, if profiling is enabled.
    //
    // Start profiling inference input load (inllwding allocating and populating
    // the parameter and swizzle RAMs), if profiling is enabled.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC_INFERENCE_PROFILING))
    {
        NNE_DESC_INFERENCE_PROFILE_END(
            &(pInference->hdr.profiling.elapsedTimes[LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PROFILING_REGION_DESC_LOAD]));

        NNE_DESC_INFERENCE_PROFILE_BEGIN(
            &(pInference->hdr.profiling.elapsedTimes[LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PROFILING_REGION_INPUT_LOAD]));
    }

    // Reset the swizzle and parameter RAM inference allocators.
    PMU_ASSERT_OK_OR_GOTO(status,
        nneSwzlRamInferenceReset(),
        s_nneDescInferenceExelwte_exit);

    // Allocate all necessary parameter and swizzle RAM for all inputs.
    PMU_ASSERT_OK_OR_GOTO(status,
        nneSwzlRamInferenceAlloc(numDescInputs * sizeof(LwU32), &swzlRamOffset),
        s_nneDescInferenceExelwte_exit);

    if (PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC_INFERENCE_PROFILING))
    {
        pInference->hdr.profiling.bMultiInferenceAllocValid =
            nneParmRamMultiInferenceAllocValid(
                    pInference->hdr.staticVarCache.parmRamSeqNum);
        NNE_DESC_INFERENCE_PROFILE_BEGIN(
            &(pInference->hdr.profiling.elapsedTimes[LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PROFILING_REGION_PARM_RAM_MULTI_INFERENCE_LOAD]));
    }

    // Check if the old allocation is still valid.
    if (!nneParmRamMultiInferenceAllocValid(
            pInference->hdr.staticVarCache.parmRamSeqNum))
    {
        // If not, reset the allocations.
        PMU_ASSERT_OK_OR_GOTO(status,
            nneParmRamMultiInferenceReset(),
            s_nneDescInferenceExelwte_exit);

        // And then allocate more memory.
        PMU_ASSERT_OK_OR_GOTO(status,
            nneParmRamMultiInferenceAlloc(
                pInference->hdr.varInputCntStatic * sizeof(LwU32),
                &pInference->hdr.staticVarCache.parmRamOffset,
                &pInference->hdr.staticVarCache.parmRamSeqNum),
            s_nneDescInferenceExelwte_exit);

        // Write the static inputs into the new allocation.
        PMU_ASSERT_OK_OR_GOTO(status,
            nneVarsVarInputsWrite(
                pInference->pVarInputsStatic,
                pInference->hdr.staticVarCache.parmRamOffset,
                pInference->hdr.varInputCntStatic),
            s_nneDescInferenceExelwte_exit);
    }

    //
    // Create the mappings from NNE_VAR index to input index for the static
    // variables.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        s_nneDescVarMapEntriesAdd(
            pInference->varMapEntries,
            LW_ARRAY_ELEMENTS(pInference->varMapEntries),
            pInference->pVarInputsStatic,
            pInference->hdr.varInputCntStatic,
            NNE_DESC_INFERENCE_VAR_INPUT_TYPE_ENUM_STATIC),
        s_nneDescInferenceExelwte_exit);

    if (PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC_INFERENCE_PROFILING))
    {
        NNE_DESC_INFERENCE_PROFILE_END(
            &(pInference->hdr.profiling.elapsedTimes[LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PROFILING_REGION_PARM_RAM_MULTI_INFERENCE_LOAD]));

        NNE_DESC_INFERENCE_PROFILE_BEGIN(
            &(pInference->hdr.profiling.elapsedTimes[LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PROFILING_REGION_PARM_RAM_INFERENCE_LOAD]));
    }

    //
    // Reset the inference allocations and allocate them after the
    // multi-inference allocations have been handled.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        nneParmRamInferenceReset(),
        s_nneDescInferenceExelwte_exit);

    varInputsLoopSize = pInference->hdr.varInputCntLoop * sizeof(LwU32);

    PMU_ASSERT_OK_OR_GOTO(status,
        nneParmRamInferenceAlloc(varInputsLoopSize * pInference->hdr.loopCnt,
                                 &parmRamLoopInputOffset),
        s_nneDescInferenceExelwte_exit);

    // Write all the loop-specific inputs to the parameter RAM.
    for (loopIdx = 0; loopIdx < pInference->hdr.loopCnt; loopIdx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            nneVarsVarInputsWrite(pInference->pLoops[loopIdx].pVarInputs,
                                  parmRamLoopInputOffset + (varInputsLoopSize * loopIdx),
                                  pInference->hdr.varInputCntLoop),
            s_nneDescInferenceExelwte_exit);

        //
        // Use the first loop to create the mappings from NNE_VAR index to input
        // index for the loop variables.
        //
        if (loopIdx == 0U)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                s_nneDescVarMapEntriesAdd(
                    pInference->varMapEntries,
                    LW_ARRAY_ELEMENTS(pInference->varMapEntries),
                    pInference->pLoops[loopIdx].pVarInputs,
                    pInference->hdr.varInputCntLoop,
                    NNE_DESC_INFERENCE_VAR_INPUT_TYPE_ENUM_LOOP),
                s_nneDescInferenceExelwte_exit);
        }
    }

    //
    // Start profiling swizzle generation and population of the swizzle RAM,
    // if profiling is enabled.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC_INFERENCE_PROFILING))
    {
        NNE_DESC_INFERENCE_PROFILE_END(
            &(pInference->hdr.profiling.elapsedTimes[LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PROFILING_REGION_PARM_RAM_INFERENCE_LOAD]));

        NNE_DESC_INFERENCE_PROFILE_BEGIN(
            &(pInference->hdr.profiling.elapsedTimes[LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PROFILING_REGION_SWZL_GENERATION]));
    }

    pLayer = NNE_LAYER_GET(pDesc->firstLayerIdx);
    if (pLayer == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_STATE;
        goto s_nneDescInferenceExelwte_exit;
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        nneLayerNumBiasGet(pLayer, &numBias),
        s_nneDescInferenceExelwte_exit);

    // Generate and write the swizzle.
    numSwzl = numDescInputs + numBias;
    for (swzlIdx = 0; swzlIdx < numSwzl; swzlIdx++)
    {
        NNE_DESC_INFERENCE_VAR_MAP_ENTRY *pVarMapEntry;
        swzlStagingIdx = swzlIdx % NNE_DESC_SWZL_STAGING_SIZE;

        // If we're lwrrently iterating over the NNE_VAR section
        if (swzlIdx < numDescInputs)
        {
            pVar = NNE_VAR_GET(pDescs->pVarIdxArray[pDesc->firstVarIdx + swzlIdx]);
            if (pVar == NULL)
            {
                PMU_TRUE_BP();
                status = FLCN_ERR_ILWALID_STATE;
                goto s_nneDescInferenceExelwte_exit;
            }

            PMU_ASSERT_OK_OR_GOTO(status,
                s_nneDescInferenceVarInputIdxGet(pInference, pVar, &pVarMapEntry),
                s_nneDescInferenceExelwte_exit);

            if (pVarMapEntry->inputType == NNE_DESC_INFERENCE_VAR_INPUT_TYPE_ENUM_LOOP)
            {
                swzlStaging[swzlStagingIdx].parmRamOffset = parmRamLoopInputOffset +
                                                            (pVarMapEntry->varIdx * sizeof(LwU32));
                swzlStaging[swzlStagingIdx].stride        = varInputsLoopSize;
            }
            else if (pVarMapEntry->inputType == NNE_DESC_INFERENCE_VAR_INPUT_TYPE_ENUM_STATIC)
            {
                swzlStaging[swzlStagingIdx].parmRamOffset = pInference->hdr.staticVarCache.parmRamOffset +
                                                            (pVarMapEntry->varIdx * sizeof(LwU32));
                swzlStaging[swzlStagingIdx].stride        = 0;
            }
            else
            {
                // _ILWALID or unknown input type specified for this variable.

                PMU_TRUE_BP();
                goto s_nneDescInferenceExelwte_exit;
            }
        }
        // If we're lwrrently iterating over the bias section of the swizzle
        else
        {
            swzlStaging[swzlStagingIdx].parmRamOffset = NNE_PARM_RAM_1_CONST_OFFSET;
            swzlStaging[swzlStagingIdx].stride        = 0;
        }

        //
        // Flush the staging buffer if it is full, or if this is the last swizzle to write
        // to swizzle RAM.
        //
        if ((swzlStagingIdx == (NNE_DESC_SWZL_STAGING_SIZE - 1)) ||
            (swzlIdx        == (numSwzl - 1)))
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                nneSwzlWrite(swzlStaging,
                             swzlRamOffset + ((swzlIdx - swzlStagingIdx) * sizeof(LwU32)),
                             swzlStagingIdx + 1),
                s_nneDescInferenceExelwte_exit);
        }
    }

    //
    // Stop profiling time taken to generate and write the swizzle to
    // swizzle RAM, as well as the total time to load all inference inputs.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC_INFERENCE_PROFILING))
    {
        NNE_DESC_INFERENCE_PROFILE_END(
            &(pInference->hdr.profiling.elapsedTimes[LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PROFILING_REGION_SWZL_GENERATION]));
        NNE_DESC_INFERENCE_PROFILE_END(
            &(pInference->hdr.profiling.elapsedTimes[LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PROFILING_REGION_INPUT_LOAD]));
    }

    // Allocate space for the inference loop statuses.
    PMU_ASSERT_OK_OR_GOTO(status,
        nneParmRamInferenceAlloc(pInference->hdr.loopCnt * sizeof(LwU32), &parmRamStatusOffset),
        s_nneDescInferenceExelwte_exit);

    // Allocate space for the inference loop outputs.
    PMU_ASSERT_OK_OR_GOTO(status,
        nneParmRamInferenceAlloc(pInference->hdr.loopCnt * pDesc->numDescOutputs * sizeof(LwU32),
                                 &parmRamOutputOffset),
        s_nneDescInferenceExelwte_exit);

    if (PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC_INFERENCE_PROFILING))
    {
        NNE_DESC_INFERENCE_PROFILE_BEGIN(
            &(pInference->hdr.profiling.elapsedTimes[LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PROFILING_REGION_DESC_INFERENCE_CONFIG]));
    }

    // Configure the descriptor to use the allocated buffers for input/output.
    PMU_ASSERT_OK_OR_GOTO(status,
        nneDescInferenceConfig(pDesc,
                               pInference->hdr.loopCnt,
                               swzlRamOffset,
                               parmRamOutputOffset,
                               parmRamStatusOffset),
        s_nneDescInferenceExelwte_exit);

    if (PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC_INFERENCE_PROFILING))
    {
        NNE_DESC_INFERENCE_PROFILE_END(
            &(pInference->hdr.profiling.elapsedTimes[LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PROFILING_REGION_DESC_INFERENCE_CONFIG]));
    }

    // Trigger the inference.
    PMU_ASSERT_OK_OR_GOTO(status,
        nneDescTriggerInferenceAndWait_HAL(&Nne,
                                           pDesc->ramState.nnetId,
                                           pInference->hdr.loopCnt,
                                           &(pInference->hdr.inferenceCfg),
                                           &(pInference->hdr.profiling.elapsedTimes[LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PROFILING_REGION_DLC_EVAL])),
        s_nneDescInferenceExelwte_exit);

    pInference->hdr.descOutputCnt = pDesc->numDescOutputs;

    // Copy out the results.
    for (loopIdx = 0; loopIdx < pInference->hdr.loopCnt; loopIdx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_nneDescLoopStatusRead(&pInference->pLoops[loopIdx].exceptions,
                                    parmRamStatusOffset + (loopIdx * sizeof(LwU32))),
            s_nneDescInferenceExelwte_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            s_nneDescsDescOutputsRead(pDesc,
                                      pInference->pLoops[loopIdx].pDescOutputs,
                                      parmRamOutputOffset + (loopIdx * pDesc->numDescOutputs * sizeof(LwU32))),
            s_nneDescInferenceExelwte_exit);
    }

s_nneDescInferenceExelwte_exit:
    // End profiling, if enabled.
    if (PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC_INFERENCE_PROFILING))
    {
        NNE_DESC_INFERENCE_PROFILE_END(
            &(pInference->hdr.profiling.elapsedTimes[LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PROFILING_REGION_TOTAL_TIME]));
    }

    return status;
}

/*!
 * @brief Initialize the RAM state for an NNE_DESC
 *
 * @param[IN/OUT] pRamState   Pointer to the NNE_DESC_RAM_STATE to initialize.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If pRamState is NULL.
 * @return FLCN_OK                     If pRamState was successfully initialized.
 */
static inline FLCN_STATUS 
s_nneDescRamStateInit
(
    NNE_DESC_RAM_STATE   *pRamState
)
{
    FLCN_STATUS  status = FLCN_OK;

    if (pRamState == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_nneDescRamStateInit_exit;
    }

    pRamState->seqId   = NNE_DESC_RAM_STATE_SEQ_ID_ILWALID;
    pRamState->nnetId  = 0;

s_nneDescRamStateInit_exit:
    return status;
}

/*!
 * @brief Get the mapping of a neural-net's input signal kind to its value's location in an inference invocation.
 *
 * This method gets the map entry specifying the location of a neural-net's input signal (i.e. NNE_VAR)
 * to its value's location for an inference vocation. Inference ilwocations specify input signal values
 * in either their static input array or their loop input array.
 *
 * Note: NNE clients specify inputs to a neural-net by ID, keeping the ordering of these inputs
 *       to the neural-net transparent to clients.
 *
 * @param[IN]  pInference   The structure holding all data for an inference invocation.
 * @param[IN]  pVar         The NNE_VAR to retrieve the location within @ref pInference for.
 * @param[OUT] ppEntry      Structure holding the mapping of a neural-net's input type (i.e. NNE_VAR) to
 *                          its location in either the static variable array or the loop-dependent variable array.
 *
 * @return  FLCN_OK                 Successfully retrieved ppEntry
 * @return  FLCN_ERR_ILWALID_STATE  @ref NNE_VAR's group index is invalid
 */
static FLCN_STATUS
s_nneDescInferenceVarInputIdxGet
(
    NNE_DESC_INFERENCE                  *pInference,
    NNE_VAR                             *pVar,
    NNE_DESC_INFERENCE_VAR_MAP_ENTRY   **ppEntry
)
{
    FLCN_STATUS status;
    const LwBoardObjIdx varGrpIdx = BOARDOBJ_GET_GRP_IDX(&pVar->super);

    PMU_ASSERT_OK_OR_GOTO(status,
        varGrpIdx < LW_ARRAY_ELEMENTS(pInference->varMapEntries) ?
            FLCN_OK : FLCN_ERR_ILWALID_STATE,
        s_nneDescInferenceVarInputIdxGet_exit);
    *ppEntry = &pInference->varMapEntries[varGrpIdx];

s_nneDescInferenceVarInputIdxGet_exit:
    return status;
}

/*!
 * @copydoc NneDescOutputParse
 */
static FLCN_STATUS
s_nneDescOutputParse
(
    LW2080_CTRL_NNE_NNE_DESC_OUTPUT_ID   *pOutputId,
    LwF32                                 data,
    LW2080_CTRL_NNE_NNE_DESC_OUTPUT      *pOutput
)
{
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pOutputId == NULL) ||
         (pOutput   == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_nneDescOutputParse_exit);

    // Set the output type.
    pOutput->type = pOutputId->type;

    //
    // Route to type specific implementation of this method for further
    // processing.
    //
    switch (pOutput->type)
    {
        case LW2080_CTRL_NNE_NNE_DESC_OUTPUT_TYPE_POWER_DN:
            return s_nneDescOutputParse_POWER_DN(pOutputId, data, pOutput);

        case LW2080_CTRL_NNE_NNE_DESC_OUTPUT_TYPE_POWER_TOTAL:
            return s_nneDescOutputParse_POWER_TOTAL(pOutputId, data, pOutput);

        case LW2080_CTRL_NNE_NNE_DESC_OUTPUT_TYPE_ENERGY_DN:
            return s_nneDescOutputParse_ENERGY_DN(pOutputId, data, pOutput);

        case LW2080_CTRL_NNE_NNE_DESC_OUTPUT_TYPE_ENERGY_TOTAL:
            return s_nneDescOutputParse_ENERGY_TOTAL(pOutputId, data, pOutput);

        case LW2080_CTRL_NNE_NNE_DESC_OUTPUT_TYPE_PERF:
            return s_nneDescOutputParse_PERF(pOutputId, data, pOutput);

        default:
            PMU_TRUE_BP();
            status = FLCN_ERR_NOT_SUPPORTED;
            goto s_nneDescOutputParse_exit;
    }

s_nneDescOutputParse_exit:
    return status;
}

/*!
 * @copydoc NneDescOutputParse
 */
static FLCN_STATUS
s_nneDescOutputParse_POWER_DN
(
    LW2080_CTRL_NNE_NNE_DESC_OUTPUT_ID   *pOutputId,
    LwF32                                 data,
    LW2080_CTRL_NNE_NNE_DESC_OUTPUT      *pOutput
)
{
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pOutputId == NULL) ||
         (pOutput   == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_nneDescOutputParse_POWER_DN_exit);

    // Read the raw dynamic-normalized power data.
    pOutput->data.powerDN.id      = pOutputId->typeSpecificId.powerDN;
    pOutput->data.powerDN.powermW = lwF32ColwertToU32(data + 0.5);

    // Normalize to 1.0[V] as this is this is the voltage that the NNE tasks operates in, by convention.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_nneDescOutputNormalizeToOneVolt(
            pOutput->data.powerDN.id.voltRailIdx,
            pOutput->data.powerDN.id.voltageuV,
            &(pOutput->data.powerDN.powermW)),
        s_nneDescOutputParse_POWER_DN_exit);

s_nneDescOutputParse_POWER_DN_exit:
    return status;
}

/*!
 * @copydoc NneDescOutputParse
 */
static FLCN_STATUS
s_nneDescOutputParse_POWER_TOTAL
(
    LW2080_CTRL_NNE_NNE_DESC_OUTPUT_ID   *pOutputId,
    LwF32                                 data,
    LW2080_CTRL_NNE_NNE_DESC_OUTPUT      *pOutput
)
{
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pOutputId == NULL) ||
         (pOutput   == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_nneDescOutputParse_POWER_TOTAL_exit);

    pOutput->data.powerTotal.id      = pOutputId->typeSpecificId.powerTotal;
    pOutput->data.powerTotal.powermW = lwF32ColwertToU32(data + 0.5);

s_nneDescOutputParse_POWER_TOTAL_exit:
    return status;
}

/*!
 * @copydoc NneDescOutputParse
 */
static FLCN_STATUS
s_nneDescOutputParse_ENERGY_DN
(
    LW2080_CTRL_NNE_NNE_DESC_OUTPUT_ID   *pOutputId,
    LwF32                                 data,
    LW2080_CTRL_NNE_NNE_DESC_OUTPUT      *pOutput
)
{
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pOutputId == NULL) ||
         (pOutput   == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_nneDescOutputParse_ENERGY_DN_exit);

    pOutput->data.energyDN.id       = pOutputId->typeSpecificId.energyDN;
    pOutput->data.energyDN.energymJ = lwF32ColwertToU32(data + 0.5);

    // Normalize to 1.0[V] as this is this is the voltage that the NNE tasks operates in, by convention.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_nneDescOutputNormalizeToOneVolt(
            pOutput->data.powerDN.id.voltRailIdx,
            pOutput->data.powerDN.id.voltageuV,
            &(pOutput->data.powerDN.powermW)),
        s_nneDescOutputParse_ENERGY_DN_exit);

s_nneDescOutputParse_ENERGY_DN_exit:
    return status;
}

/*!
 * @copydoc NneDescOutputParse
 */
static FLCN_STATUS
s_nneDescOutputParse_ENERGY_TOTAL
(
    LW2080_CTRL_NNE_NNE_DESC_OUTPUT_ID   *pOutputId,
    LwF32                                 data,
    LW2080_CTRL_NNE_NNE_DESC_OUTPUT      *pOutput
)
{
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pOutputId == NULL) ||
         (pOutput   == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_nneDescOutputParse_ENERGY_TOTAL_exit);

    pOutput->data.energyTotal.id       = pOutputId->typeSpecificId.energyTotal;
    pOutput->data.energyTotal.energymJ = lwF32ColwertToU32(data + 0.5);

s_nneDescOutputParse_ENERGY_TOTAL_exit:
    return status;
}

/*!
 * @copydoc NneDescOutputParse
 */
static FLCN_STATUS
s_nneDescOutputParse_PERF
(
    LW2080_CTRL_NNE_NNE_DESC_OUTPUT_ID   *pOutputId,
    LwF32                                 data,
    LW2080_CTRL_NNE_NNE_DESC_OUTPUT      *pOutput
)
{
    LwU32         lw2Pow12Fp32 = LW_2_POW_12_F32;
    LwF32         perfF32;
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pOutputId == NULL) ||
         (pOutput   == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_nneDescOutputParse_PERF_exit);

    pOutput->data.perf.perfId = pOutputId->typeSpecificId.perf;

    //
    // The following algorithm colwerts an IEEE-754 floating point
    // number to a LwUFXP20_12, which is assumed to be represented as an LwU32,
    // with the binary-point between the bits at index 11 and 12. A left shift of the
    // LwF32 value would make it so that the binary-point is at the correct location
    // when the LwF32 is casted to a LwU32.
    //

    // perfF32 = data << 12
    perfF32 = lwF32Mul(data, lwF32MapFromU32(&lw2Pow12Fp32));

    // Type cast from FP32 to U32 and assign to FXP20_12.
    pOutput->data.perf.perf = lwF32ColwertToU32(perfF32);

s_nneDescOutputParse_PERF_exit:
    return status;
}

/*!
 * @brief   Reads the exception status at a given paramter RAM offset
 *
 * @param[out]  pExceptions     Exception state at the RAM offset
 * @param[in]   paramRamOffset  Offset from which to read status
 *
 * @return  FLCN_OK Success
 * @retuen  Others  Propogated from callees
 */
static FLCN_STATUS
s_nneDescLoopStatusRead
(
    LW2080_CTRL_NNE_NNE_DESC_INFERENCE_FP_EXCEPTION_FLAGS  *pExceptions,
    LwU32                                                   parmRamOffset
)
{
    FLCN_STATUS status;
    LwU32 data;

    // Read the data from RAM.
    PMU_ASSERT_OK_OR_GOTO(status,
        nneParmRamAccess_HAL(&Nne,
                             &data,
                             parmRamOffset,
                             sizeof(data),
                             LW_FALSE),
        s_nneDescLoopStatusRead_exit);

    *pExceptions = nneFpExceptionsParse_HAL(&Nne, data);

s_nneDescLoopStatusRead_exit:
    return status;
}

/*!
 * @brief Parse in an array of NNE_DESC_OUTPUT at a parameter RAM location.
 *
 * @param[IN]     pDesc           Descriptor to parse outputs for.
 * @param[OUT]    pOutputs        Array of NNE_DESC_OUTPUT to parse
 * @param[IN]     parmRamOffset   Offset into the parameter RAM to start parsing the
 *                                outputs from.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If pOutputs is NULL
 * @return Other errors                If internal functions return an error.
 * @return FLCN_OK                     If the outputs were successfully read and parsed.
 */
static FLCN_STATUS
s_nneDescsDescOutputsRead
(
    NNE_DESC                           *pDesc,
    LW2080_CTRL_NNE_NNE_DESC_OUTPUT    *pOutputs,
    LwU32                               parmRamOffset
)
{
    LwU32         idx;
    LwU32         data;
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    if ((pDesc == NULL) ||
        (pOutputs == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneParmRamDescOutputsRead_exit;
    }

    //
    // Read the outputs and parse them one-by-one. The additional overhead of doing this
    // is acceptable because we expect there to be very few outputs (e.g. 2).
    //
    for (idx = 0; idx < pDesc->numDescOutputs; idx++)
    {
        // Read the binary IEEE-754 value from RAM.
        PMU_ASSERT_OK_OR_GOTO(status,
            nneParmRamAccess_HAL(&Nne,
                                 &data,
                                 parmRamOffset + (idx * sizeof(LwU32)),
                                 sizeof(LwU32),
                                 LW_FALSE),
            nneParmRamDescOutputsRead_exit);

        // Parse it.
        PMU_ASSERT_OK_OR_GOTO(status,
            s_nneDescOutputParse(
                &(pDesc->pDescOutputs[idx]),
                lwF32MapFromU32(&data),
                &(pOutputs[idx])),
            nneParmRamDescOutputsRead_exit);
    }

nneParmRamDescOutputsRead_exit:
    return status;
}

/*!
 * @brief Helper to normalize a power or energy value to 1.0[V].
 *
 * @param[IN]     voltRailIdx   Index into VOLT_RAILS of the rail to normalize to 1.0[V] for.
 * @param[IN]     origVoltuV    Original voltage that @ref pValue is normalized to.
 * @param[IN/OUT] pValue        Power/energy value to normalize to 1.0[V].
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pValue is NULL.
 * @return FLCN_ERR_ILWALID_STATE      If @ref voltRailIdx does not point to a valid VOLT_RAIL object.
 * @return FLCN_OK                     If @ref pValue was successfully normalized to 1.0[V].
 */
static FLCN_STATUS
s_nneDescOutputNormalizeToOneVolt
(
    LwBoardObjIdx   voltRailIdx,
    LwU32           origVoltuV,
    LwU32          *pValue
)
{
    VOLT_RAIL              *pVoltRail;
    LwBoardObjIdx           dynamicEquIdx;
    PWR_EQUATION_DYNAMIC   *pDyn;
    LwUFXP52_12             tmpUFXP52_12;
    LwU64                   tmpU64;
    FLCN_STATUS             status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        (pValue == NULL) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_nneDescOutputNormalizeToOneVolt_exit);

    //
    // If @ref pValue is already normalized to 1V, early return. This is NOT
    // an error.
    //
    if (origVoltuV == 1000000)
    {
        goto s_nneDescOutputNormalizeToOneVolt_exit;
    }

    // Compute the scaling factor.
    pVoltRail = VOLT_RAIL_GET(voltRailIdx);
    PMU_ASSERT_OK_OR_GOTO(status,
        (pVoltRail == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_nneDescOutputNormalizeToOneVolt_exit);

    dynamicEquIdx = voltRailVoltScaleExpPwrEquIdxGet(pVoltRail);
    PMU_ASSERT_OK_OR_GOTO(status,
        (dynamicEquIdx == LW2080_CTRL_PMGR_PWR_EQUATION_INDEX_ILWALID) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_nneDescOutputNormalizeToOneVolt_exit);

    pDyn = (PWR_EQUATION_DYNAMIC *)PWR_EQUATION_GET(dynamicEquIdx);
    PMU_ASSERT_OK_OR_GOTO(status,
        (pDyn == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_nneDescOutputNormalizeToOneVolt_exit);

    //
    // Numerical Analysis:
    // Assume maximum voltage of 2.0[V] = 2000000[uV] => 21-bits
    // 2000000 << 12 => 33-bits
    //
    // Division by 1000000 to scale for units will only decrease bit width. Ergo,
    // maximum bit-width for the voltage is 33-bits, which fits in the 52-bit wide
    // integer portion of UFXP52_12 type.
    //
    tmpUFXP52_12 = LW_TYPES_U64_TO_UFXP_X_Y_SCALED(52, 12, origVoltuV, 1000000);
    PMU_ASSERT_OK_OR_GOTO(status,
        (LW_TYPES_UFXP_X_Y_TO_U64(52, 12, tmpUFXP52_12) > LW_TYPES_UFXP_INTEGER_MAX(20, 12)) ?
            FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_nneDescOutputNormalizeToOneVolt_exit);
    tmpUFXP52_12 = (LwUFXP52_12)pwrEquationDynamicScaleVoltagePower(pDyn, (LwUFXP20_12)tmpUFXP52_12);

    //
    // Divide out the voltage scaling factor from @ref pValue.
    //
    // Numerical Analysis:
    // Assume max power and energy values of 1000[W] = 1000000[mW] and 1000[J] = 1000000[mJ]
    //
    // 1000000 => 20-bits
    // 1000000 << 12 => 32-bits
    //
    // Division will only reduce maximum bit-width. Ergo, the maximum size of tmpU64
    // at any time is 32-bits. To be safe, express pValue as a 64-bit integer.
    //
    tmpU64 = ((LwU64)*pValue) << 12;
    lwU64Div(&tmpU64, &tmpU64, &tmpUFXP52_12);
    PMU_ASSERT_OK_OR_GOTO(status,
        (LwU64_HI32(tmpU64) != 0) ? FLCN_ERROR : FLCN_OK,
        s_nneDescOutputNormalizeToOneVolt_exit);
    *pValue = LwU64_LO32(tmpU64);

s_nneDescOutputNormalizeToOneVolt_exit:
    return status;
}

/*!
 * @copydoc s_nneDescVarMapEntriesAdd
 */
static FLCN_STATUS
s_nneDescVarMapEntriesAdd
(
    NNE_DESC_INFERENCE_VAR_MAP_ENTRY *pEntries,
    LwLength numEntries,
    LW2080_CTRL_NNE_NNE_VAR_INPUT *pInputs,
    LwLength numInputs,
    NNE_DESC_INFERENCE_VAR_INPUT_TYPE_ENUM inputType
)
{
    FLCN_STATUS status;
    LwLength i;
    NNE_VARS_VAR_ID_MAP *const pVarIdMap =
        NNE_VARS_VAR_ID_MAP_GET(NNE_VARS_GET());

    for (i = 0U; i < numInputs; i++)
    {
        LwBoardObjIdx varGrpIdx;

        PMU_ASSERT_OK_OR_GOTO(status,
            nneVarsVarIdMapInputToVarIdx(
                pVarIdMap,
                &pInputs[i],
                &varGrpIdx),
            s_nneDescVarMapEntriesAdd_exit);

        // Clients are allowed to specify variables that don't actually exist.
        if (varGrpIdx == LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
        {
            continue;
        }

        PMU_ASSERT_OK_OR_GOTO(status,
            varGrpIdx < numEntries ? FLCN_OK : FLCN_ERR_ILWALID_STATE,
            s_nneDescVarMapEntriesAdd_exit);

        pEntries[varGrpIdx].varIdx = i;
        pEntries[varGrpIdx].inputType = inputType;
    }

s_nneDescVarMapEntriesAdd_exit:
    return status;
}
