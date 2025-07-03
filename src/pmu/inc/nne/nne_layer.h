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
 * @file    nne_layer.h
 * @brief   Neural Net Engine (NNE) layer class interface
 */

#ifndef NNE_LAYER_H
#define NNE_LAYER_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobj.h"
#include "ctrl/ctrl2080/ctrl2080nne.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct NNE_LAYER_RAM_STATE  NNE_LAYER_RAM_STATE;

typedef struct NNE_LAYER            NNE_LAYER, NNE_LAYER_BASE;

typedef struct NNE_LAYERS           NNE_LAYERS;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @memberof NNE_LAYERS
 *
 * @public
 *
 * @brief Retrieve a pointer to the set of all NNE layers (i.e. NNE_LAYERS*)
 *
 * @return  A pointer to the set of all NNE layers casted to NNE_LAYERS* type
 * @return  NULL if the NNE_LAYERS PMU feature has been disabled
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_NNE_LAYER))
#define NNE_LAYERS_GET()                                                       \
    (&(Nne.layers))
#else
#define NNE_LAYERS_GET()                                                       \
    ((NNE_LAYERS *)NULL)
#endif

/*!
 * @memberof NNE_LAYERS
 *
 * @public
 *
 * @copydoc BOARDOBJGRP_GRP_GET()
 */
#define BOARDOBJGRP_DATA_LOCATION_NNE_LAYER                                    \
    (&(NNE_LAYERS_GET()->super.super))

/*!
 * @memberof NNE_LAYERS
 *
 * @public
 *
 * @brief Retrieve a pointer to a particular NNE_LAYER in the set of all NNE_LAYERS
 * by index.
 *
 * @param[in]  _idx     Index into NNE_LAYERS of requested NNE_LAYER object
 *
 * @return  A pointer to the NNE_LAYER at the requested index.
 */
#define NNE_LAYER_GET(_idx)                                                    \
    ((NNE_LAYER *)BOARDOBJGRP_OBJ_GET(NNE_LAYER, (_idx)))

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief DLC RAM state for an NNE_LAYER.
 */
struct NNE_LAYER_RAM_STATE
{
    /*!
     * @brief Sequence ID used to determine if this layer is resident in DLC RAM.
     *
     * @protected
     */
    LwU32                                       seqId;

    /*!
     * @brief Offset into the swizzle RAM that this layer's swizzle is stored.
     */
    LwU32                                       swzlRamOffset;

    /*!
     * @brief Offset into the parameter RAM that this layer should output to.
     */
    LwU32                                       parmRamOutputOffset;

    /*!
     * @brief Offset into the weight RAM for that this layer's weights are stored.
     */
    LwU32                                       weightRamOffset;

    /*!
     * @brief Size of the allocation in the weight RAM for this layer's weights.
     */
    LwU32                                       weightRamSize;

    /*
     * @brief Offset in the weight RAM for this layer's activation_function offset
     */
    LwU32                                       weightRamActivationFuncOffset;
};

/*!
 * @brief Virtual class representing a single neural-net layer
 *
 * NNE_LAYER represents a single layer of a NNE compatible neural-net
 *
 * @extends BOARDOBJ
 */
struct NNE_LAYER
{
    /*!
     * @brief Base class.
     *
     * Must be first element of the structure to allow casting to parent class.
     *
     * @protected
     */
    BOARDOBJ                                    super;

    /*!
     * @brief DLC RAM state for this layer.
     */
    NNE_LAYER_RAM_STATE                         ramState;

    /*!
     * @brief Index into NNE_LAYERS of the layer that this one feeds into.
     *
     * @protected
     */
    LwBoardObjIdx                               nextLayerIdx;

    /*!
     * @brief Index into NNE_LAYERS of the layer that feeds into this one.
     *
     * @protected
     */
    LwBoardObjIdx                               prevLayerIdx;

    /*!
     * @brief Index into NNE_WEIGHTS for first weight in layer.
     *
     * Index into NNE_WEIGHTS for the first weight in this layer. The
     * number of weights in the layer is determined by the kind of layer
     * this is.
     *
     * @protected
     */
    LwU16                                       weightIdx;

    /*!
     * @brief Data type of the weights in this layer.
     * @ref LW2080_CTRL_NNE_NNE_LAYER_WEIGHT_TYPE_ENUM
     *
     * @protected
     */
    LW2080_CTRL_NNE_NNE_LAYER_WEIGHT_TYPE_ENUM  weightType;
};

/*!
 * @brief Container of all neural-net layers for all neural-nets used by NNE.
 *
 * @extends BOARDOBJGRP_E255
 */
struct NNE_LAYERS
{
    /*!
     * @brief Base class.
     *
     * Must be first element of the structure to allow casting to parent class.
     *
     * @protected
     */
    BOARDOBJGRP_E255   super;

    /*!
     * @ brief number of FP32 weights.
     */
    LwU16 numFp32Weights;

    /*!
     * @ brief number of FP32 weights.
     */
    LwU16 numFp16Weights;
};

/*!
 * @brief Get the number of neurons in the specified layer.
 *
 * @param[IN]  pLayer        Layer to get the number of neurons for.
 * @param[OUT] pNumNeurons   Number of neurons in the layer.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If either @ref pLayer or @ref pNumNeurons is NULL.
 * @return FLCN_OK                     If the number of neurons was successfully retrieved
 */
#define NneLayerNumNeuronsGet(fname) FLCN_STATUS (fname)(NNE_LAYER *pLayer, LwU32 *pNumNeurons)

/*!
 * @brief Get the number of inputs for the specified layer.
 *
 * @param[IN]  pLayer       Layer to get the number of inputs for.
 * @param[OUT] pNumInputs   Number of inputs that the layer takes in.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pLayer or @ref pNumInputs is NULL.
 * @return FLCN_OK                     If the number of inputs for @ref pLayer was successfully retrieved.
 */
#define NneLayerNumInputsGet(fname) FLCN_STATUS (fname)(NNE_LAYER *pLayer, LwU32 *pNumInputs)

/*!
 * @brief Get the activation function for the layer.
 *
 * @param[IN]  pLayer                Layer to get the activation function for.
 * @param[OUT] pActivationFunction   The activation function type.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT If @ref pLayer or @ref pActivationFunction is NULL.
 * @return FLCN_ERR_ILWALID_STATE    If an invalid layer type was detected.
 * @return FLCN_OK                   If the activation function was successfully retrieved.
 */
#define NneLayerActivationFunctionGet(fname) FLCN_STATUS (fname)(NNE_LAYER *pLayer, LW2080_CTRL_NNE_NNE_LAYER_FC_10_ACTIVATION_FUNCTION_TYPE_ENUM *pActivationFunction)

/*!
 * @brief Get the offset into the parameter RAM that this layer's activation
 *        function parameters are loaded at.
 *
 * @param[IN]  pLayer                      Layer to get the activation function offset for.
 * @param[OUT] pActivationFunctionOffset   Offset into the parameter RAM that the activation function
 *                                         of @ref pLayer is stored at.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pLayer or @ref pActivationFunctionOffset are NULL.
 * @return FLCN_OK                     If the activation function offset was successfully retrieved.

 */
#define NneLayerActivationFunctionOffsetGet(fname) FLCN_STATUS (fname)(NNE_LAYER *pLayer, LwU32 *pActivationFunctionOffset)

/*!
 * @brief Get the number of weights in the layer.
 *
 * @param[IN]  pLayer        Layer to get the number of weights for.
 * @param[OUT] pNumWeights   Number of weights in the layer.
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT   If @ref pLayer or @ref pNumWeights is NULL.
 * @return  FLCN_OK                     If the number of weights was successfully retrieved.
 * @return  Others                      Errors returned by callees
 */
#define NneLayerNumWeightsPerNeuronGet(fname) FLCN_STATUS (fname)(NNE_LAYER *pLayer, LwU32 *pNumWeights)

/*!
 * @brief Get the number of bias values per neuron in the layer.
 *
 * @param[IN]  pLayer     Layer to get the number of biases for.
 * @param[OUT] pNumBias   Number of biases in the layer.
 *
 * @return   FLCN_ERR_ILWALID_ARGUMENT   If @ref pLayer or @ref pNumBias is NULL.
 * @return   FLCN_OK                     If the number of biases was successfully retrieved.
 */
#define NneLayerNumBiasGet(fname) FLCN_STATUS (fname)(NNE_LAYER *pLayer, LwU8 *pNumBias)

/*!
 * @brief Get the size of the activation function for this layer
 *
 * @param[IN]  pLayer               Layer to get the weight type for.
 * @param[OUT] pActivationFuncSize  The size of the activation function.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pLayer or @ref pActivationFuncSize are NULL.
 * @return FLCN_OK                     If the size was successfully retrieved.
 */
#define NneLayerActivationFuncSizeGet(fname) FLCN_STATUS (fname)(NNE_LAYER *pLayer, LwU32 *pActivationFuncSize)

/*!
 * @brief Get the size of the activation function for this layer
 *
 * @param[IN]  pLayer               Layer to get the weight type for.
 * @param[OUT] pActivationFuncSize  The size of the activation function.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pLayer or @ref pLeakyReluSlope are NULL.
 * @return FLCN_OK                     If the size was successfully retrieved.
 */
#define NneLayerLoadActivationFunction(fname) FLCN_STATUS (fname)(NNE_LAYER *pLayer)

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler    (nneLayerBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_nneBoardObj", "nneLayerBoardObjGrpIfaceModel10Set");

// Board object interfaces.
BoardObjGrpIfaceModel10ObjSet        (nneLayerGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_nneBoardObj", "nneLayerGrpIfaceModel10ObjSet_SUPER");

/*!
 * @brief Check if a NNE_LAYER has been loaded into DLC RAM's.
 *
 * @param[IN]  pDescs       Pointer to global NNE_DESCS.
 * @param[IN]  pLayer       Layer to check for residency.
 * @param[OUT] pBIsLoaded   Boolean denoting if the layer has been loaded.
 *
 * @return LW_TRUE    If @ref pLayer is loaded.
 * @return LW_FALSE   If @ref pLayer is NOT loaded.
 *
 * @memberof NNE_LAYERS
 *
 * @public
 */
FLCN_STATUS nneLayersLayerIsLoaded(NNE_LAYERS *pLayers, NNE_LAYER *pLayer, LwBool *pBIsLoaded)
    GCC_ATTRIB_SECTION("imem_nne", "nneLayersLayerIsLoaded");

/*!
 * @brief Load this layer's weights into the DLC weight RAM.
 *
 * @param[IN] seqId    Sequence ID of NNE_DESCS. Use to determine if this layer has
 *                     already been loaded.
 * @param[IN] pLayer   Layer to load the weights for.
 *
 * @return FLCN_OK   If the weights of the requested layer have been successfully loaded.
 */
FLCN_STATUS nneLayersLayerLoad(NNE_LAYERS *pLayers, NNE_LAYER *pLayer)
    GCC_ATTRIB_SECTION("imem_nne", "nneLayersLayerWeightLoad");

/*!
 * @copydoct NneLayerNumNeuronsGet
 */
NneLayerNumNeuronsGet (nneLayerNumNeuronsGet)
    GCC_ATTRIB_SECTION("imem_nne", "nneLayerNumNeuronsGet");

/*!
 * @copydoc NneLayerNumInputsGet
 */
NneLayerNumInputsGet (nneLayerNumInputsGet)
    GCC_ATTRIB_SECTION("imem_nne", "nneLayerNumInputsGet");

/*!
 * @copydoc NneLayerActivationFunctionGet
 */
NneLayerActivationFunctionGet (nneLayerActivationFunctionGet)
    GCC_ATTRIB_SECTION("imem_nne", "nneLayerActivationFunctionGet");

/*!
 * @copydoc NneLayerActivationFunctionOffsetGet
 */
NneLayerActivationFunctionOffsetGet (nneLayerActivationFunctionOffsetGet)
    GCC_ATTRIB_SECTION("imem_nne", "nneLayerActivationFunctionOffsetGet");

/*!
 * @brief Get the swizzle RAM offset for the specified layer.
 *
 * @param[IN] pLayer   Layer to get the swizzle RAM offset for.
 *
 * @return Swizzle RAM offset that this layer's swizzle is at.
 */
LwU32 nneLayerSwizzleRamOffsetGet(NNE_LAYER *pLayer)
    GCC_ATTRIB_SECTION("imem_nne", "nneLayerSwizzleRamOffsetGet");

/*!
 * @brief Get the weight RAM offset for the specified layer.
 *
 * @param[IN] pLayer              Layer to get the weight RAM offset for.
 * @param[OUT] pWeightRamOffset   Offset into the weight RAM that this layer's weights reside at.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pLayer or @ref pWeightRamOffset is NULL.
 * @return FLCN_OK                     If the weight RAM offset for this layer was successfully
 *                                     retrieved.
 */
FLCN_STATUS nneLayerWeightRamOffsetGet(NNE_LAYER *pLayer, LwU32 *pWeightRamOffset)
    GCC_ATTRIB_SECTION("imem_nne", "nneLayerWeightRamOffsetGet");

/*!
 * @brief Get the weight type for the specified layer.
 *
 * @param[IN]  pLayer        Layer to get the weight type for.
 * @param[OUT] pWeightType   Weight type of the layer.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pLayer or @ref pWeightType are NULL.
 * @return FLCN_OK                     If the weight type was successfully retrieved.
 */
FLCN_STATUS nneLayerWeightTypeGet(NNE_LAYER *pLayer, LW2080_CTRL_NNE_NNE_LAYER_WEIGHT_TYPE_ENUM *pWeightType)
    GCC_ATTRIB_SECTION("imem_nne", "nneLayerWeightTypeGet");

/*!
 * @copydoc NneLayerNumWeightsPerNeuronGet
 */
NneLayerNumWeightsPerNeuronGet (nneLayerNumWeightsPerNeuronGet)
    GCC_ATTRIB_SECTION("imem_nne", "nneLayerNumWeightsPerNeuronGet");

/*!
 * @copydoc NneLayerNumBiasGet
 */
NneLayerNumBiasGet (nneLayerNumBiasGet)
    GCC_ATTRIB_SECTION("imem_nne", "nneLyaerNumBiasGet");

/*!
 * @copydoc NneLayerLActivationFuncSizeGet
 */
NneLayerActivationFuncSizeGet (nneLayerActivationFuncSizeGet)
    GCC_ATTRIB_SECTION("imem_nne", "nneLayerActivationFuncSizeGet");

/*!
 * @copydoc NneLayerLoadActivationFunction
 */
NneLayerLoadActivationFunction (nneLayerLoadActivationFunction)
    GCC_ATTRIB_SECTION("imem_nne", "nneLayerLoadActivationFunction");


/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief Get the weight RAM size for the specified layer.
 *
 * @param[in]   pLayer          Layer to get the weight RAM size for.
 * @param[out]  pWeightRamSize  Size of the allocation in the weight RAM for this layer's weights.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT    If @ref pLayer or @ref pWeightRamSize is NULL.
 * @return FLCN_OK                      If the weight RAM size for this layer was successfully
 *                                      retrieved.
 */
static inline FLCN_STATUS
nneLayerWeightRamSizeGet
(
    NNE_LAYER *pLayer,
    LwU32 *pWeightRamSize
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        (pLayer != NULL) && (pWeightRamSize != NULL) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        nneLayerWeightRamSizeGet_exit);

    *pWeightRamSize = pLayer->ramState.weightRamSize;

nneLayerWeightRamSizeGet_exit:
    return status;
}

/* ------------------------ Include Derived Types --------------------------- */
#include "nne/nne_layer_fc_10.h"

#endif // NNE_LAYER_H
