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
 * @file   nne_layer.c
 * @brief  Neural Net Engine (NNE) layer methods.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "pmu/ssurface.h"

/* ------------------------ Application Includes ---------------------------- */
#include "objnne.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "nne/nne_weight_ram.h"
#include "main.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjGrpIfaceModel10SetHeader              (s_nneLayerIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_nneBoardObj", "s_nneLayerIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry               (s_nneLayerIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_nneBoardObj", "s_nneLayerIfaceModel10SetEntry");
static FLCN_STATUS s_nneLayerRamStateInit(NNE_LAYER_RAM_STATE *pRamState)
    GCC_ATTRIB_SECTION("imem_nne", "s_nneLayerRamStateInit");

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
#define NNE_LAYER_WEIGHTS_STAGING_SIZE_BYTES         (DMA_MIN_READ_ALIGNMENT * 4)
#define NNE_LAYER_WEIGHTS_STAGING_SIZE_DWORDS        ((NNE_LAYER_WEIGHTS_STAGING_SIZE_BYTES) / sizeof(LwU32))
#define NNE_LAYER_RAM_STATE_ILWALID_VALUE            (LW_U32_MAX)
/* ------------------------ Global Variables -------------------------------- */

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief NNE_LAYERS implementation of BoardObjGrpIfaceModel10CmdHandler().
 *
 * @memberof NNE_LAYERS
 *
 * @public
 *
 * @copydetails BoardObjGrpIfaceModel10CmdHandler()
 */
FLCN_STATUS
nneLayerBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status = FLCN_OK;

    PMU_ASSERT_OK_OR_GOTO(status,
        BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E255,            // _grpType
            NNE_LAYER,                                  // _class
            pBuffer,                                    // _pBuffer
            s_nneLayerIfaceModel10SetHeader,                  // _hdrFunc
            s_nneLayerIfaceModel10SetEntry,                   // _entryFunc
            ga10xAndLater.nne.nneLayerGrpSet,           // _ssElement
            OVL_INDEX_DMEM(nne),                        // _ovlIdxDmem
            BoardObjGrpVirtualTablesNotImplemented),    // _ppObjectVtables
        nneLayerBoardObjGrpIfaceModel10Set_exit);

nneLayerBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * @brief Constructor.
 *
 * @memberof NNE_LAYER
 *
 * @protected
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
nneLayerGrpIfaceModel10ObjSet_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_NNE_LAYER   *pDescLayer = (RM_PMU_NNE_LAYER *)pBoardObjDesc;
    NNE_LAYER          *pLayer;
    FLCN_STATUS         status;

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc),
        nneLayerGrpIfaceModel10ObjSet_SUPER_exit);

    pLayer = (NNE_LAYER *)*ppBoardObj;

    // Set member variables
    pLayer->nextLayerIdx = pDescLayer->nextLayerIdx;
    pLayer->prevLayerIdx = pDescLayer->prevLayerIdx;
    pLayer->weightIdx    = pDescLayer->weightIdx;
    pLayer->weightType   = pDescLayer->weightType;

    // Initialize the RAM state
    PMU_ASSERT_OK_OR_GOTO(status,
        s_nneLayerRamStateInit(&(pLayer->ramState)),
        nneLayerGrpIfaceModel10ObjSet_SUPER_exit);

nneLayerGrpIfaceModel10ObjSet_SUPER_exit:
    return status;
}

/*!
 * @copydoc nneLayersLayerIsLoaded
 */
FLCN_STATUS
nneLayersLayerIsLoaded
(
    NNE_LAYERS  *pLayers,
    NNE_LAYER   *pLayer,
    LwBool      *pIsLoaded
)
{
    LwU32         seqId;
    NNE_DESCS    *pDescs = NNE_DESCS_GET();
    FLCN_STATUS   status = FLCN_OK;

    if ((pLayers   == NULL) ||
        (pLayer    == NULL) ||
        (pIsLoaded == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneLayersLayerIsLoaded_exit;
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        nneDescsSeqIdGet(pDescs, &seqId),
        nneLayersLayerIsLoaded_exit);

    *pIsLoaded = (seqId == pLayer->ramState.seqId);

nneLayersLayerIsLoaded_exit:
    return status;
}

/*!
 * @copydoc nneLayersLayerWeightsLoad
 */
FLCN_STATUS
nneLayersLayerLoad
(
    NNE_LAYERS *pLayers,
    NNE_LAYER  *pLayer
)
{
    LwU32         surfBaseOffset;
    LwU32         surfLwrOffset;
    LwU32         surfEndOffset;
    LwU32         dmaStart;
    LwU32         dmaSize;
    LwU32         maxWeights;
    LwU32         bufValidStartByte;
    LwU32         numWeightsPerNeuron;
    LwU32         numWeights;
    LwU32         numNeurons;
    LwU32         bufValidDataSize = 0;
    LwU32         activationFuncSize;
    LwBool        bLoaded;
    FLCN_STATUS   status           = FLCN_OK;

    //
    // This anonymous union is used as a staging buffer for copying data from the
    // super-surface, into DMEM, and finally into the weight RAM. It includes a
    // dword variant of the data strictly to align to dword boundaries that the PMU
    // DMA engine follows.
    //
    union
    {
        LwU8    data[NNE_LAYER_WEIGHTS_STAGING_SIZE_BYTES];
        LwU32   alignmentDword[NNE_LAYER_WEIGHTS_STAGING_SIZE_DWORDS];
    } weightsStaging;

    // Sanity checking.
    if (pLayer == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneLayersLayerLoad_exit;
    }

    //
    // If this layer is already loaded, exit. A loaded layer is assumed
    // to have its swizzle, weights, and parameters loaded.
    // Note that this does not include loading the layer into descriptor RAM.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        nneLayersLayerIsLoaded(pLayers, pLayer, &bLoaded),
        nneLayersLayerLoad_exit);

    if (bLoaded)
    {
        goto nneLayersLayerLoad_exit;
    }

    // Set all weight-type specific information.
    PMU_ASSERT_OK_OR_GOTO(status,
        nneLayerNumWeightsPerNeuronGet(pLayer, &numWeightsPerNeuron),
        nneLayersLayerLoad_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        nneLayerNumNeuronsGet(pLayer, &numNeurons),
        nneLayersLayerLoad_exit);

    numWeights = numWeightsPerNeuron * numNeurons;

    switch (pLayer->weightType)
    {
        case LW2080_CTRL_NNE_NNE_LAYER_WEIGHT_TYPE_FP16:
            surfBaseOffset =
                RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.nne.nneLayerWeightArrays.fp16WeightArray[pLayer->weightIdx]);
            maxWeights     = LW2080_CTRL_NNE_NNE_LAYER_WEIGHTS_MAX_FP16;
            pLayer->ramState.weightRamSize = numWeights * sizeof(LwU16);
            break;
        case LW2080_CTRL_NNE_NNE_LAYER_WEIGHT_TYPE_FP32:
            surfBaseOffset =
                RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.nne.nneLayerWeightArrays.fp32WeightArray[pLayer->weightIdx]);
            maxWeights     = LW2080_CTRL_NNE_NNE_LAYER_WEIGHTS_MAX_FP32;
            pLayer->ramState.weightRamSize = numWeights * sizeof(LwU32);
            break;
        default:
            PMU_TRUE_BP();
            status = FLCN_ERR_ILWALID_STATE;
            goto nneLayersLayerLoad_exit;
    }

    // Check if reading in the weights for this layer would exceed the weight array's end.
    if ((numWeights > maxWeights) ||
        (pLayer->weightIdx > (maxWeights - numWeights)))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_OUT_OF_RANGE;
        goto nneLayersLayerLoad_exit;
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        nneLayerActivationFuncSizeGet(pLayer, &activationFuncSize),
        nneLayersLayerLoad_exit);

    //
    // Allocate necessary amount of weightRam needed to hold:
    // weightRamSize (size of the weights) + activationFuncSize (size of the activation function)
    //

    PMU_ASSERT_OK_OR_GOTO(status,
        nneWeightRamAlloc(pLayer->ramState.weightRamSize + activationFuncSize,
                          &(pLayer->ramState.weightRamOffset)),
        nneLayersLayerLoad_exit);

    // The activation function will always have an offset in the weightRam after all of the weights of the layer
    pLayer->ramState.weightRamActivationFuncOffset = pLayer->ramState.weightRamSize + pLayer->ramState.weightRamOffset;

    // Copy the weights, chunk by chunk, from the super-surface to the the weight RAM.
    surfLwrOffset = surfBaseOffset;
    surfEndOffset = surfBaseOffset + pLayer->ramState.weightRamSize;

    while (surfLwrOffset < surfEndOffset)
    {
        // Copy the weights from the super surface to the @ref weightsStaging buffer.
        dmaStart = LW_ALIGN_DOWN(surfLwrOffset, DMA_MIN_READ_ALIGNMENT);
        dmaSize  = LW_MIN(LW_ALIGN_UP(surfEndOffset, DMA_MIN_READ_ALIGNMENT) - dmaStart,
                                      NNE_LAYER_WEIGHTS_STAGING_SIZE_BYTES);

        PMU_ASSERT_OK_OR_GOTO(status,
            ssurfaceRd(&(weightsStaging.data), dmaStart, dmaSize),
            nneLayersLayerLoad_exit);

        // Copy the staged weights from @ref weightsStaging into the weight RAM.
        bufValidStartByte = surfLwrOffset - dmaStart;
        bufValidDataSize  = LW_MIN(dmaStart + dmaSize, surfEndOffset) - surfLwrOffset;

        PMU_ASSERT_OK_OR_GOTO(status,
            nneWeightRamWrite_HAL(&Nne,
                                  &(weightsStaging.data[bufValidStartByte]),
                                  pLayer->ramState.weightRamOffset + (surfLwrOffset - surfBaseOffset),
                                  bufValidDataSize),
            nneLayersLayerLoad_exit);

        // Increment the current offset we have copied up until.
        surfLwrOffset += bufValidDataSize;
    }

    // Load the activation function
    PMU_ASSERT_OK_OR_GOTO(status,
        nneLayerLoadActivationFunction(pLayer),
        nneLayersLayerLoad_exit);

nneLayersLayerLoad_exit:
    return status;
}

/*!
 * @copydoc NneLayerNumNeuronsGet
 */
FLCN_STATUS
nneLayerNumNeuronsGet
(
    NNE_LAYER   *pLayer,
    LwU32       *pNumNeurons
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if ((pLayer      == NULL) ||
        (pNumNeurons == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneLayerNumNeuronsGet_exit;
    }

    switch (pLayer->super.type)
    {
        case LW2080_CTRL_NNE_NNE_LAYER_TYPE_FC_10:
            PMU_ASSERT_OK_OR_GOTO(status,
                nneLayerNumNeuronsGet_FC_10(pLayer, pNumNeurons),
                nneLayerNumNeuronsGet_exit);
            break;

        default:
            PMU_TRUE_BP();
            status = FLCN_ERR_ILWALID_STATE;
            goto nneLayerNumNeuronsGet_exit;
            break;
    }

nneLayerNumNeuronsGet_exit:
    return status;
}

/*!
 * @copydoc NneLayerNumInputsGet
 */
FLCN_STATUS
nneLayerNumInputsGet
(
    NNE_LAYER   *pLayer,
    LwU32       *pNumInputs
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if ((pLayer     == NULL) ||
        (pNumInputs == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneLayerNumInputsGet_exit;
    }

    switch (pLayer->super.type)
    {
        case LW2080_CTRL_NNE_NNE_LAYER_TYPE_FC_10:
            PMU_ASSERT_OK_OR_GOTO(status,
                nneLayerNumInputsGet_FC_10(pLayer, pNumInputs),
                nneLayerNumInputsGet_exit);
            break;

        default:
            PMU_TRUE_BP();
            status = FLCN_ERR_ILWALID_STATE;
            goto nneLayerNumInputsGet_exit;
            break;
    }

nneLayerNumInputsGet_exit:
    return status;
}

/*!
 * @copydoc NneLayerActivationFunctionGet
 */
FLCN_STATUS
nneLayerActivationFunctionGet
(
    NNE_LAYER   *pLayer,
    LW2080_CTRL_NNE_NNE_LAYER_FC_10_ACTIVATION_FUNCTION_TYPE_ENUM
                *pActivationFunction
)
{
    FLCN_STATUS status = FLCN_OK;
    // Sanity checking.
    if ((pLayer              == NULL) ||
        (pActivationFunction == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneLayerActivationFunctionGet_exit;
    }

    switch (pLayer->super.type)
    {
        case LW2080_CTRL_NNE_NNE_LAYER_TYPE_FC_10:
            PMU_ASSERT_OK_OR_GOTO(status,
                nneLayerActivationFunctionGet_FC_10(pLayer, pActivationFunction),
                nneLayerActivationFunctionGet_exit);
            break;

        default:
            PMU_TRUE_BP();
            status = FLCN_ERR_ILWALID_STATE;
            goto nneLayerActivationFunctionGet_exit;
            break;
    }

nneLayerActivationFunctionGet_exit:
    return status;
}

/*!
 * @copydoc NneLayerActivationFunctionOffsetGet
 */
FLCN_STATUS
nneLayerActivationFunctionOffsetGet
(
    NNE_LAYER   *pLayer,
    LwU32       *pActivationFunctionOffset
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if ((pLayer                    == NULL) ||
        (pActivationFunctionOffset == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneLayerActivationFunctionOffsetGet_exit;
    }

    switch (pLayer->super.type)
    {
        case LW2080_CTRL_NNE_NNE_LAYER_TYPE_FC_10:
            PMU_ASSERT_OK_OR_GOTO(status,
                nneLayerActivationFunctionOffsetGet_FC_10(pLayer, pActivationFunctionOffset),
                nneLayerActivationFunctionOffsetGet_exit);
            break;

        default:
            PMU_TRUE_BP();
            status = FLCN_ERR_ILWALID_STATE;
            goto nneLayerActivationFunctionOffsetGet_exit;
            break;
    }

nneLayerActivationFunctionOffsetGet_exit:
    return status;
}

/*!
 * @copydoc nneLayerWeightRamOffsetGet
 */
FLCN_STATUS
nneLayerWeightRamOffsetGet
(
    NNE_LAYER   *pLayer,
    LwU32       *pWeightRamOffset
)
{
    FLCN_STATUS status = FLCN_OK;

    if ((pLayer           == NULL) ||
        (pWeightRamOffset == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneLayerWeightRamOffsetGet_exit;
    }

    *pWeightRamOffset = pLayer->ramState.weightRamOffset;

nneLayerWeightRamOffsetGet_exit:
    return status;
}

/*!
 * @copydoc nneLayerWeightTypeGet
 */
FLCN_STATUS
nneLayerWeightTypeGet
(
    NNE_LAYER                                    *pLayer,
    LW2080_CTRL_NNE_NNE_LAYER_WEIGHT_TYPE_ENUM   *pWeightType
)
{
    FLCN_STATUS status = FLCN_OK;

    if ((pLayer      == NULL) ||
        (pWeightType == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneLayerWeightTypeGet_exit;
    }

    *pWeightType = pLayer->weightType;

nneLayerWeightTypeGet_exit:
    return status;
}

/*!
 * @copydoc NneLayerNumWeightsPerNeuronGet
 */
FLCN_STATUS
nneLayerNumWeightsPerNeuronGet
(
    NNE_LAYER   *pLayer,
    LwU32       *pNumWeights
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if ((pLayer      == NULL) ||
        (pNumWeights == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneLayerNumWeightsPerNeuronGet_exit;
    }

    switch (pLayer->super.type)
    {
        case LW2080_CTRL_NNE_NNE_LAYER_TYPE_FC_10:
            PMU_ASSERT_OK_OR_GOTO(status,
                nneLayerNumWeightsPerNeuronGet_FC_10(pLayer, pNumWeights),
                nneLayerNumWeightsPerNeuronGet_exit);
            break;

        default:
            PMU_TRUE_BP();
            status = FLCN_ERR_ILWALID_STATE;
            goto nneLayerNumWeightsPerNeuronGet_exit;
            break;
    }

nneLayerNumWeightsPerNeuronGet_exit:
    return status;
}

/*!
 * @copydoc NneLayerNumBiasGet
 */
FLCN_STATUS
nneLayerNumBiasGet
(
    NNE_LAYER   *pLayer,
    LwU8        *pNumBias
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if ((pLayer   == NULL) ||
        (pNumBias == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneLayerNumBiasGet_exit;
    }

    switch (pLayer->super.type)
    {
        case LW2080_CTRL_NNE_NNE_LAYER_TYPE_FC_10:
            PMU_ASSERT_OK_OR_GOTO(status,
                nneLayerNumBiasGet_FC_10(pLayer, pNumBias),
                nneLayerNumBiasGet_exit);
            break;

        default:
            PMU_TRUE_BP();
            status = FLCN_ERR_ILWALID_STATE;
            goto nneLayerNumBiasGet_exit;
            break;
    }

nneLayerNumBiasGet_exit:
    return status;
}

/*!
 * @copydoc NneLayerActivationFuncSizeGet
 */
FLCN_STATUS
nneLayerActivationFuncSizeGet
(
    NNE_LAYER   *pLayer,
    LwU32       *pActivationFuncSize
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if ((pLayer   == NULL) ||
        (pActivationFuncSize == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneLayerActivationFuncSizeGet_exit;
    }

    switch (pLayer->super.type)
    {
        case LW2080_CTRL_NNE_NNE_LAYER_TYPE_FC_10:
            PMU_ASSERT_OK_OR_GOTO(status,
                nneLayerActivationFuncSizeGet_FC_10(pLayer, pActivationFuncSize),
                nneLayerActivationFuncSizeGet_exit);
            break;

        default:
            PMU_TRUE_BP();
            status = FLCN_ERR_ILWALID_STATE;
            goto nneLayerActivationFuncSizeGet_exit;
            break;
    }

nneLayerActivationFuncSizeGet_exit:
    return status;
}

/*!
 * @copydoc NneLayerLoadActivationFunction
 */
FLCN_STATUS
nneLayerLoadActivationFunction
(
    NNE_LAYER   *pLayer
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if ((pLayer   == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneLayerLoadActivationFunction_exit;
    }

    switch (pLayer->super.type)
    {
        case LW2080_CTRL_NNE_NNE_LAYER_TYPE_FC_10:
            PMU_ASSERT_OK_OR_GOTO(status,
                nneLayerLoadActivationFunction_FC_10(pLayer),
                nneLayerLoadActivationFunction_exit);
            break;

        default:
            PMU_TRUE_BP();
            status = FLCN_ERR_ILWALID_STATE;
            goto nneLayerLoadActivationFunction_exit;
            break;
    }

nneLayerLoadActivationFunction_exit:
    return status;
}


/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10SetHeader()
 *
 * @memberof NNE_LAYERS
 *
 * @private
 */
static FLCN_STATUS
s_nneLayerIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    NNE_LAYERS                                          *pLayers;
    const RM_PMU_NNE_NNE_LAYER_BOARDOBJGRP_SET_HEADER   *pSet =
        (const RM_PMU_NNE_NNE_LAYER_BOARDOBJGRP_SET_HEADER *)pHdrDesc;
    FLCN_STATUS                                          status;

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc),
        s_nneLayerIfaceModel10SetHeader_exit);

    pLayers = (NNE_LAYERS *)pBObjGrp;

    // Copy global NNE_LAYER state.
    pLayers->numFp32Weights = pSet->numFp32Weights;
    pLayers->numFp16Weights = pSet->numFp16Weights;

s_nneLayerIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry()
 *
 * @memberof NNE_LAYERS
 *
 * @private
 */
static FLCN_STATUS
s_nneLayerIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_NNE_NNE_LAYER_TYPE_FC_10:
            status = nneLayerGrpIfaceModel10ObjSet_FC_10(pModel10, ppBoardObj,
                       sizeof(NNE_LAYER_FC_10), pBuf); // NJ??
            break;
        default:
            PMU_TRUE_BP();
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
    }

    return status;
}

/*!
 * @brief Initialize the RAM state for an NNE_LAYER.
 *
 * @param[IN/OUT] pRamState    Pointer to the NNE_LAYER_RAM_STATE to initialize.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If pRamState is NULL.
 * @return FLCN_OK                     If pRamState was successfully initialized.
 */
static inline FLCN_STATUS
s_nneLayerRamStateInit
(
    NNE_LAYER_RAM_STATE   *pRamState
)
{
    FLCN_STATUS status = FLCN_OK;
    // Sanity checking.
    if (pRamState == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_nneLayerRamStateInit_exit;
    }

    pRamState->seqId                         = NNE_DESC_RAM_STATE_SEQ_ID_ILWALID;
    pRamState->swzlRamOffset                 = NNE_LAYER_RAM_STATE_ILWALID_VALUE;
    pRamState->parmRamOutputOffset           = NNE_LAYER_RAM_STATE_ILWALID_VALUE;
    pRamState->weightRamOffset               = NNE_LAYER_RAM_STATE_ILWALID_VALUE;
    pRamState->weightRamSize                 = NNE_LAYER_RAM_STATE_ILWALID_VALUE;
    pRamState->weightRamActivationFuncOffset = NNE_LAYER_RAM_STATE_ILWALID_VALUE;

s_nneLayerRamStateInit_exit:
    return status;
}
