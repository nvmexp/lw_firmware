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
 * @file    nne_layer_fc_10.h
 * @brief   Neural Net Engine (NNE) fully connected layer class, version 1.0
 */

#ifndef NNE_LAYER_FC_10_H
#define NNE_LAYER_FC_10_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "nne/nne_layer.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct NNE_LAYER_FC_10 NNE_LAYER_FC_10;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief  Class representing a fully-connected layer.
 *
 * A fully connected layer is defined as one where each neuron takes in as input
 * all of the previous layer's neurons.
 *
 * @extends NNE_LAYER
 */
struct NNE_LAYER_FC_10
{
    /*!
     * @brief Base class.
     *
     * Must be first element of the structure to allow casting to parent class.
     *
     * @protected
     */
    NNE_LAYER                                                       super;

    /*!
     * @brief Number of input neurons to this layer.
     *
     * @protected
     */
    LwU16                                                           numInputs;

    /*!
     * @brief Number of neurons in this layer.
     *
     * @protected
     */
    LwU16                                                           numNeurons;

    /*!
     * @brief Activation type to be applied to this layer
     * @ref LW2080_CTRL_NNE_NNE_LAYER_FC_10_ACTIVATION_FUNCTION_TYPE_ENUM
     *
     * @protected
     */
    LW2080_CTRL_NNE_NNE_LAYER_FC_10_ACTIVATION_FUNCTION_TYPE_ENUM   activationFunction;

    /*!
     * @brief Leaky Rectifier Linear Unit (ReLU) slope.
     * LwU32 blind-cast of ReLU slope. Only used when the activation function
     * is leaky ReLU.
     *
     * @protected
     */
    LwU32                                                           leakyReLUSlope;

    /*!
     * @deprecated -- This should be part of NNE_LAYER_RAM_STATE as a weightRam
     *                param
     *
     * @brief Parameter RAM offset that this layer's activation function parameters
     *        are stored at, if applicable.
     */
    LwU32                                                           paramRamActivationOffset;

    /*!
     * @brief Boolean specifying the application of a bias to this layer
     *
     * Specifies if this layer's neurons apply a bias. The bias values, if
     * present, exist in the weight array. Setting this Boolean causes NNE
     * to interpret the weight array in a way that assumes that the biases
     * reside alongside the weights.
     */
    LwBool                                                          bHasBias;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet    (nneLayerGrpIfaceModel10ObjSet_FC_10)
    GCC_ATTRIB_SECTION("imem_nneBoardObj", "nneLayerGrpIfaceModel10ObjSet_FC_10");

/*!
 * @copydoc NneLayerNumNeuronsGet
 */
NneLayerNumNeuronsGet (nneLayerNumNeuronsGet_FC_10)
    GCC_ATTRIB_SECTION("imem_nne", "nneLayerNumNeuronsGet_FC_10");

/*!
 * @copydoc NneLayerNumInputsGet
 */
NneLayerNumInputsGet (nneLayerNumInputsGet_FC_10)
    GCC_ATTRIB_SECTION("imem_nne", "nneLayerNumInputsGet_FC_10");

/*!
 * @copydoc NneLayerActivationFunctionGet
 */
NneLayerActivationFunctionGet (nneLayerActivationFunctionGet_FC_10)
    GCC_ATTRIB_SECTION("imem_nne", "nneLayerActivationFunctionGet_FC_10");

/*!
 * @deprecated no longer used as the information is in NNE_LAYER_RAM_STATE
 * @copydoc NneLayerActivationFunctionOffsetGet
 */
NneLayerActivationFunctionOffsetGet (nneLayerActivationFunctionOffsetGet_FC_10)
    GCC_ATTRIB_SECTION("imem_nne", "nneLayerActivationFunctionOffsetGet_FC_10");

/*!
 * @copydoc NneLayerNumWeightsPerNeuronGet
 */
NneLayerNumWeightsPerNeuronGet (nneLayerNumWeightsPerNeuronGet_FC_10)
    GCC_ATTRIB_SECTION("imem_nne", "nneLayerNumWeightsPerNeuronGet_FC_10");

/*!
 * @copydoc NneLayerNumBiasGet
 */
NneLayerNumBiasGet (nneLayerNumBiasGet_FC_10)
    GCC_ATTRIB_SECTION("imem_nne", "nneLayerNumBiasGet_FC_10");

/*!
 * @copydoc NneLayerLActivationFuncSizeGet
 */
NneLayerActivationFuncSizeGet (nneLayerActivationFuncSizeGet_FC_10)
    GCC_ATTRIB_SECTION("imem_nne", "nneLayerActivationFuncSizeGet_FC_10");

/*!
 * @copydoc NneLayerLoadActivationFunction
 */
NneLayerLoadActivationFunction (nneLayerLoadActivationFunction_FC_10)
    GCC_ATTRIB_SECTION("imem_nne", "nneLayerLoadActivationFunction_FC_10");

#endif // NNE_LAYER_FC_10_H
