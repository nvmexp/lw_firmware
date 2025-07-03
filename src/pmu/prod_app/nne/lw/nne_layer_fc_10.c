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
 * @file    nne_layer_fc_10.c
 * @brief   NNE_LAYER_FC_10 class implementation.
 *
 * Version 1.0 of the implementation of a Fully Connected (FC) layer
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "objnne.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */

/*!
 * @memberof NNE_LAYER_FC_10
 *
 * @public
 *
 * @brief Constructor.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
nneLayerGrpIfaceModel10ObjSet_FC_10
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_NNE_LAYER_FC_10 *pDescLayerFc10 = (RM_PMU_NNE_LAYER_FC_10 *)pBoardObjDesc;
    NNE_LAYER_FC_10        *pNneLayerFc10;
    FLCN_STATUS             status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDescLayerFc10->activationFunction != LW2080_CTRL_NNE_NNE_LAYER_FC_10_ACTIVATION_FUNCTION_TYPE_LEAKY_RELU ||
         pDescLayerFc10->super.weightType == LW2080_CTRL_NNE_NNE_LAYER_WEIGHT_TYPE_FP32),
         FLCN_ERR_ILWALID_ARGUMENT,
         nneLayerGrpIfaceModel10ObjSet_FC_10_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        nneLayerGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        nneLayerGrpIfaceModel10ObjSet_FC_10_exit);

    pNneLayerFc10 = (NNE_LAYER_FC_10 *)*ppBoardObj;

    // Set member variables.
    pNneLayerFc10->numInputs          = pDescLayerFc10->numInputs;
    pNneLayerFc10->numNeurons         = pDescLayerFc10->numNeurons;
    pNneLayerFc10->activationFunction = pDescLayerFc10->activationFunction;
    pNneLayerFc10->leakyReLUSlope     = pDescLayerFc10->leakyReLUSlope;
    pNneLayerFc10->bHasBias           = pDescLayerFc10->bHasBias;

nneLayerGrpIfaceModel10ObjSet_FC_10_exit:
    return status;
}

/*!
 * @copydoc NneLayerNumNeuronsGet
 */
FLCN_STATUS
nneLayerNumNeuronsGet_FC_10
(
    NNE_LAYER   *pLayer,
    LwU32       *pNumNeurons
)
{
    NNE_LAYER_FC_10 *pLayerFc10 = (NNE_LAYER_FC_10 *)pLayer;
    FLCN_STATUS      status     = FLCN_OK;

    // Sanity checking.
    if ((pLayerFc10  == NULL) ||
        (pNumNeurons == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneLayerNumNeuronsGet_FC_10_exit;
    }

    *pNumNeurons = pLayerFc10->numNeurons;

nneLayerNumNeuronsGet_FC_10_exit:
    return status;
}

/*!
 * @copydoc NneLayerNumInputsGet
 */
FLCN_STATUS
nneLayerNumInputsGet_FC_10
(
    NNE_LAYER     *pLayer,
    LwU32        *pNumInputs
)
{
    NNE_LAYER_FC_10 *pLayerFc10 = (NNE_LAYER_FC_10 *)pLayer;
    FLCN_STATUS      status     = FLCN_OK;

    // Sanity checking.
    if ((pLayerFc10 == NULL) ||
        (pNumInputs == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneLayerNumInputsGet_FC_10_exit;
    }

    *pNumInputs = pLayerFc10->numInputs;

nneLayerNumInputsGet_FC_10_exit:
    return status;
}

/*!
 * @copydoc NneLayerActivationFunctionGet
 */
FLCN_STATUS
nneLayerActivationFunctionGet_FC_10
(
    NNE_LAYER                                                       *pLayer,
    LW2080_CTRL_NNE_NNE_LAYER_FC_10_ACTIVATION_FUNCTION_TYPE_ENUM   *pActivationFunction
)
{
    NNE_LAYER_FC_10 *pLayerFc10 = (NNE_LAYER_FC_10 *)pLayer;
    FLCN_STATUS      status     = FLCN_OK;

    // Sanity checking.
    if ((pLayerFc10          == NULL) ||
        (pActivationFunction == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneLayerActivationFunctionGet_FC_10_exit;
    }

    *pActivationFunction = pLayerFc10->activationFunction;

nneLayerActivationFunctionGet_FC_10_exit:
    return status;
}

/*!
 * @copydoc NneLayerActivationFunctionOffsetGet
 */
FLCN_STATUS
nneLayerActivationFunctionOffsetGet_FC_10
(
    NNE_LAYER   *pLayer,
    LwU32       *pActivationFunctionOffset
)
{
    NNE_LAYER_FC_10 *pLayerFc10 = (NNE_LAYER_FC_10 *)pLayer;
    FLCN_STATUS      status     = FLCN_OK;

    // Sanity checking.
    if ((pLayerFc10                == NULL) ||
        (pActivationFunctionOffset == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneLayerActivationFunctionOffsetGet_FC_10_exit;
    }

    *pActivationFunctionOffset = pLayerFc10->super.ramState.weightRamActivationFuncOffset;

nneLayerActivationFunctionOffsetGet_FC_10_exit:
    return status;
}

/*!
 * @copydoc NneLayerNumWeightsPerNeuronGet
 */
FLCN_STATUS
nneLayerNumWeightsPerNeuronGet_FC_10
(
    NNE_LAYER   *pLayer,
    LwU32       *pNumWeights
)
{
    NNE_LAYER_FC_10 *pLayerFc10 = (NNE_LAYER_FC_10 *)pLayer;
    FLCN_STATUS      status     = FLCN_OK;
    LwU8             numBias;

    // Sanity checking.
    if ((pLayerFc10  == NULL) ||
        (pNumWeights == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneLayerNumWeightsPerNeuronGet_FC_10_exit;
    }

    //
    // The true bias is specified by a weight, to be multipled against a
    // constant of 1 in the parameter RAM, so add the number of biases into the
    // number of weights for each neuron.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        nneLayerNumBiasGet_FC_10(pLayer, &numBias),
        nneLayerNumWeightsPerNeuronGet_FC_10_exit);

    *pNumWeights = pLayerFc10->numInputs + numBias;

nneLayerNumWeightsPerNeuronGet_FC_10_exit:
    return status;
}

/*!
 * @copydoc NneLayerNumBiasGet
 */
FLCN_STATUS
nneLayerNumBiasGet_FC_10
(
    NNE_LAYER   *pLayer,
    LwU8        *pNumBias
)
{
    NNE_LAYER_FC_10 *pLayerFc10 = (NNE_LAYER_FC_10 *)pLayer;
    FLCN_STATUS      status     = FLCN_OK;

    // Sanity checking.
    if ((pLayerFc10  == NULL) ||
        (pNumBias    == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneLayerNumBiasGet_FC_10_exit;
    }

    *pNumBias = (pLayerFc10->bHasBias ? 1 : 0);

nneLayerNumBiasGet_FC_10_exit:
    return status;
}

/*!
 * @copydoc NneLayerActivationFuncSizeGet
 */
FLCN_STATUS
nneLayerActivationFuncSizeGet_FC_10
(
    NNE_LAYER   *pLayer,
    LwU32       *pActivationFuncSize
)
{
    NNE_LAYER_FC_10 *pLayerFc10 = (NNE_LAYER_FC_10 *)pLayer;
    FLCN_STATUS      status     = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pLayerFc10             != NULL) ||
        (pActivationFuncSize    != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        nneLayerActivationFuncSizeGet_FC_10_exit);

    //TODO: Need to update the function to support FP16 sizes

    // If the activation function is Leaky RelU then set the return size to be the size of a LwU32
    if (pLayerFc10->activationFunction == LW2080_CTRL_NNE_NNE_LAYER_FC_10_ACTIVATION_FUNCTION_TYPE_LEAKY_RELU)
    {
        *pActivationFuncSize = sizeof(LwU32);
    }
    else
    {
        *pActivationFuncSize = 0;
    }

nneLayerActivationFuncSizeGet_FC_10_exit:
    return status;
}

/*!
 * @copydoc NneLayerLoadActivationFunction
 */
FLCN_STATUS
nneLayerLoadActivationFunction_FC_10
(
    NNE_LAYER   *pLayer
)
{
    NNE_LAYER_FC_10 *pLayerFc10 = (NNE_LAYER_FC_10 *)pLayer;
    FLCN_STATUS      status     = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pLayerFc10 != NULL ||
         pLayerFc10->super.ramState.weightRamActivationFuncOffset != 0),
        FLCN_ERR_ILWALID_ARGUMENT,
        nneLayerLoadActivationFunction_FC_10_exit);

    //TODO: Need to update the function to support FP16 activation function paramater writes to the weightRam

    // If the activation function is Leaky RelU then set the return size to be the size of a LwU32
    if (pLayerFc10->activationFunction == LW2080_CTRL_NNE_NNE_LAYER_FC_10_ACTIVATION_FUNCTION_TYPE_LEAKY_RELU)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            nneWeightRamWrite_HAL(&Nne,
                                  (LwU8*)&(pLayerFc10->leakyReLUSlope),
                                  pLayerFc10->super.ramState.weightRamActivationFuncOffset,
                                  (sizeof(LwU32))),
            nneLayerLoadActivationFunction_FC_10_exit);
    }

nneLayerLoadActivationFunction_FC_10_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
