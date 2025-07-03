/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file nne_desc_client.h
 * @brief Neural-net inference client interface.
 */

#ifndef NNE_DESC_CLIENT_H
#define NNE_DESC_CLIENT_H

/* ------------------------ System Includes --------------------------------- */
#include "ctrl/ctrl2080/ctrl2080nne.h"
#include "nne/nne_var.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
typedef struct NNE_DESC_INFERENCE_COMPLETE_NOTIFICATION NNE_DESC_INFERENCE_COMPLETE_NOTIFICATION;

typedef struct NNE_DESC_INFERENCE                       NNE_DESC_INFERENCE;

typedef struct NNE_DESC_INFERENCE_LOOP                  NNE_DESC_INFERENCE_LOOP;

typedef struct NNE_DESC_INFERENCE_VAR_MAP_ENTRY         NNE_DESC_INFERENCE_VAR_MAP_ENTRY;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @defgroup    NNE_DESC_INFERENCE_VAR_INPUT_TYPE_ENUM
 *
 * Indicates the "type" of an input to the an inference, i.e., static, per-loop,
 * or, if not specified, invalid.
 *
 * NNE_DESC_INFERENCE_VAR_INPUT_TYPE_ENUM_ILWALID - Indicates input is
 * invalid/unspecified
 *
 * NNE_DESC_INFERENCE_VAR_INPUT_TYPE_ENUM_STATIC - Indicates input is a
 * static/non-loop input to the inference.
 *
 * NNE_DESC_INFERENCE_VAR_INPUT_TYPE_ENUM_LOOP - Indicates input is a loop
 * input to the inference.
 *
 * @{
 */
typedef LwU8 NNE_DESC_INFERENCE_VAR_INPUT_TYPE_ENUM;
#define NNE_DESC_INFERENCE_VAR_INPUT_TYPE_ENUM_ILWALID  0U
#define NNE_DESC_INFERENCE_VAR_INPUT_TYPE_ENUM_STATIC   1U
#define NNE_DESC_INFERENCE_VAR_INPUT_TYPE_ENUM_LOOP     2U
/*!@}*/

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief Structure holding per-inference loop inputs/outputs
 */
struct NNE_DESC_INFERENCE_LOOP
{
    /*!
     * @brief [IN] Array holding per-inference loop inputs.
     *
     * Has @ref NNE_DESC_INFERENCE::hdr.varInputCntLoop elements in the array.
     */
    LW2080_CTRL_NNE_NNE_VAR_INPUT                          *pVarInputs;

    /*!
     * @brief [OUT] Exceptions that oclwrred during exelwtion of the loop.
     */
    LW2080_CTRL_NNE_NNE_DESC_INFERENCE_FP_EXCEPTION_FLAGS   exceptions;

    /*!
     * @brief [OUT] Array holding per-infernece loop outputs.
     *
     * Has @ref NNE_DESC_INFERENCE::hdr.descOutputCnt elements.
     */
    LW2080_CTRL_NNE_NNE_DESC_OUTPUT                        *pDescOutputs;
};

/*!
 * @brief Mapping of a neural-net input signal to its value's location in an inference request.
 */
struct NNE_DESC_INFERENCE_VAR_MAP_ENTRY
{
    /*!
     * @brief Index into either the static variable array or the loop variable array that
     *        a neural-net signal exists.
     */
    LwU32    varIdx;

    /*!
     * @brief Indicates the type of the input used for the variable.
     */
    NNE_DESC_INFERENCE_VAR_INPUT_TYPE_ENUM inputType;
};

/*!
 * @brief Interface structure used by PMU-based NNE clients to ilwoke NNE inference.
 */
struct NNE_DESC_INFERENCE
{
    /*!
     * @brief [IN] Handle to queue that NNE will send completion signal to.
     */
    LwrtosQueueHandle                           syncQueueHandle;

    /*!
     * @brief [IN] Array of variables that remain constant across inference loops.
     *
     * There are @ref inputCntStatic elements in this array.
     */
    LW2080_CTRL_NNE_NNE_VAR_INPUT              *pVarInputsStatic;

    /*!
     * @brief [IN/OUT] Array of per-inference loop inputs and outputs.
     *
     * There are @ref inputCntLoop elements in this array.
     */
    NNE_DESC_INFERENCE_LOOP                    *pLoops;

    /*!
     * @brief [IN/OUT] Header containing all metadata.
     *
     * @copydoc LW2080_CTRL_NNE_NNE_DESC_INFERENCE_HEADER
     */
    LW2080_CTRL_NNE_NNE_DESC_INFERENCE_HEADER   hdr;

    /*!
     * @brief [IN/OUT] Array of structures that indicate to which input index an
     *        @ref NNE_VAR corresponds. Indexed via the @ref NNE_VAR object's
     *        @ref LwBoardObjIdx
     */
    NNE_DESC_INFERENCE_VAR_MAP_ENTRY            varMapEntries[LW2080_CTRL_NNE_NNE_VAR_MAX];

    /*!
     * @brief [IN] DMEM overlay that this structure is contained in.
     *
     * For easy reference of NNE clients to retrieve the DMEM overlay that
     * their instance of this structure was instantiated in.
     */
    LwU8                                        dmemOvl;
};

/*!
 * @brief Packet sent back from NNE to the client confirming inference completion.
 */
struct NNE_DESC_INFERENCE_COMPLETE_NOTIFICATION
{
    /*!
     * Status of the inference request.
     */
    FLCN_STATUS   inferenceStatus;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Client interface to allocate and initialize an inference structure.
 *
 * PMU-based clients run inference via NNE by passing a structure containing all
 * inference input parameters and outputs to NNE. This function allocates and
 * initializes that structure.
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
 * @return FLCN_ERR_ILWALID_ARGUMENT   if @pInference is NULL or a parameter is out-of-bounds.
 * @return other errors                if callees return an error.
 *
 * @memberof NNE_DESC
 */
FLCN_STATUS nneDescInferenceInit(NNE_DESC_INFERENCE *pInference, LwU8 dmemOvl,
                                 LwU8 nneDescIdx, LwU16 inputCntStatic,
                                 LwU16 inputCntLoop, LwU8 loopCnt,
                                 LwrtosQueueHandle syncQueueHandle)
    GCC_ATTRIB_SECTION("imem_nneInferenceClient", "nneDescInferenceInit");

/*!
 * @brief Client interface to allocate and initialize an inference structure with maximum dimensions.
 *
 * This interface allocates an NNE_DESC_INFERENCE structure of maximum dimenions, and not for a particular
 * NNE_DESC. It is intended to be ilwoked for allocating the RPC inference buffering during init.
 *
 * @param[OUT]   pInference          Allocated NNE_DESC_INFERENCE structure to initialize.
 * @param[IN]    dmemOvl             DMEM overlay that @ref pInference was allocated in.
 *
 * @return FLCN_OK                     if the requested inference structure was successfully allocated.
 * @return FLCN_ERR_ILWALID_ARGUMENT   if @pInference is NULL or a parameter is out-of-bounds.
 * @return other errors                if callees return an error.
 *
 * @memberof NNE_DESC
 */
FLCN_STATUS nneDescInferenceInitMax(NNE_DESC_INFERENCE *pInference, LwU8 dmemOvl)
    GCC_ATTRIB_SECTION("imem_nneInit", "nneDescInferenceInitMax");

/*!
 * @brief Client side interface to ilwoke an NNE inference.
 *
 * @param[IN/OUT]   pInference   NNE_DESC_INFERENCE structure holding all inputs/outputs
 *                               of the requested inference, including of the inference itself.
 *
 * @return FLCN_OK   if the inference request to NNE was serviced. Note that this
 *                   does NOT mean the inference was successful, just that NNE received the
 *                   request and serviced it.
 * @return other     there was an error in act of preparing the inference request to NNE or
 *                   an error receiving the inference completion response.
 *
 * @memberof NNE_DESC
 */
FLCN_STATUS nneDescInferenceClientEvaluate(NNE_DESC_INFERENCE *pInference)
    GCC_ATTRIB_SECTION("imem_nneInferenceClient", "nneDescInferenceClientEvaluate");

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief   Ilwalidates the mappings in @ref NNE_DESC_INFERENCE::varMapEntries
 *
 * @param[in]   pInference  Pointer to inference structure in which to
 *                          ilwalidate the entries.
 */
static inline FLCN_STATUS
nneDescInferenceVarMapIlwalidate
(
    NNE_DESC_INFERENCE *pInference
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        pInference != NULL ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        nneDescInferenceVarMapIlwalidate_exit);
    //
    // Zero the struct, and ensure this means that the entries will be marked
    // invalid.
    //
    ct_assert(NNE_DESC_INFERENCE_VAR_INPUT_TYPE_ENUM_ILWALID == 0U);
    memset(
        pInference->varMapEntries,
        0,
        sizeof(pInference->varMapEntries));

nneDescInferenceVarMapIlwalidate_exit:
    return status;
}

/* ------------------------ Include Derived Types --------------------------- */

#endif // NNE_DESC_CLIENT_H
