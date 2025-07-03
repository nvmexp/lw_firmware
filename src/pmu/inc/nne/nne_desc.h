/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file nne_desc.h
 * @brief Neural-net descriptor class interface
 */

#ifndef NNE_DESC_H
#define NNE_DESC_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "ctrl/ctrl2080/ctrl2080nne.h"
#include "task_nne.h"
#include "nne/nne_desc_client.h"
#include "lib_lwf32.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct NNE_DESC_BUFFER      NNE_DESC_BUFFER;

typedef struct NNE_DESC_RAM_STATE   NNE_DESC_RAM_STATE;

typedef struct NNE_DESC             NNE_DESC, NNE_DESC_BASE;

typedef struct NNE_DESCS_RAM_STATE  NNE_DESCS_RAM_STATE;

typedef struct NNE_DESCS            NNE_DESCS;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @memberof NNE_DESCS
 *
 * @public
 *
 * @brief Retrieve a pointer to the set of all NNE descriptors (i.e. NNE_DESCS*).
 *
 * @return  A pointer to the set of all NNE descriptors casted to NNE_DESCS* type
 * @return  NULL if the NNE_DESCS PMU feature has been disabled
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC))
#define NNE_DESCS_GET()                                                        \
    (&(Nne.descs))
#else
#define NNE_DESCS_GET()                                                       \
    ((NNE_DESCS *)NULL)
#endif

/*!
 * @memberof NNE_DESCS
 *
 * @public
 *
 * @copydoc BOARDOBJGRP_GRP_GET()
 */
#define BOARDOBJGRP_DATA_LOCATION_NNE_DESC                                     \
    (&(NNE_DESCS_GET()->super.super))

/*!
 * @memberof NNE_DESCS
 *
 * @public
 *
 * @brief Retrieve a pointer to a particular NNE_DESC in the set of all NNE_DESCS
 * by index.
 *
 * @param[in] _idx     Index into NNE_DESCS of requested NNE_DESC object
 *
 * @return  A pointer to the NNE_DESC at the requested index.
 */
#define NNE_DESC_GET(_idx)                                                     \
    ((NNE_DESC *)BOARDOBJGRP_OBJ_GET(NNE_DESC, (_idx)))

/*!
 * @brief Invalid sequence ID
 *
 * Objects holding this ID are always considered not loaded on the DLC/
 */
#define NNE_DESC_RAM_STATE_SEQ_ID_ILWALID                           (LW_U32_MAX)

/*!
 * @brief Macros identifying the buffers used to store intermediate results
 *        produced and consumed by all hidden layers.
 */
#define NNE_DESC_BUFFER_FRONT_IDX                                            (0)
#define NNE_DESC_BUFFER_BACK_IDX                                             (1)
#define NNE_DESC_BUFFER_NUM_BUFFERS                                          (2)

/*!
 * @brief   List of overlay descriptors needed for inference, beyond the base NNE
 *          descriptor.
 */
#define OSTASK_OVL_DESC_DEFINE_NNE_INFERENCE                                   \
    OSTASK_OVL_DESC_DEFINE_LIB_LW_F32

/*!
 * @brief Begins profiling a section of code.
 * @memberof NNE_DESC
 * @private
 *
 * @copydoc PERF_CHANGE_SEQ_PROFILE_BEGIN
 */
#define NNE_DESC_INFERENCE_PROFILE_BEGIN(pTimens)                              \
do {                                                                           \
    FLCN_TIMESTAMP __timensLwrr;                                               \
    LwU64          __timensResult;                                             \
                                                                               \
    osPTimerTimeNsLwrrentGet(&__timensLwrr);                                   \
                                                                               \
    LwU64_ALIGN32_UNPACK(&__timensResult, (pTimens));                          \
    lw64Sub_MOD(&__timensResult, &__timensResult, &__timensLwrr.data);         \
    LwU64_ALIGN32_PACK((pTimens), &__timensResult);                            \
} while (LW_FALSE)

/*!
 * @brief Ends profiling a secion of code.
 * @memberof NNE_DESC
 * @private
 *
 * @copydoc PERF_CHANGE_SEQ_PROFILE_END
 */
#define NNE_DESC_INFERENCE_PROFILE_END(pTimens)                                \
do {                                                                           \
    FLCN_TIMESTAMP __timensLwrr;                                               \
    LwU64          __timensResult;                                             \
                                                                               \
    osPTimerTimeNsLwrrentGet(&__timensLwrr);                                   \
                                                                               \
    LwU64_ALIGN32_UNPACK(&__timensResult, (pTimens));                          \
    lw64Add_MOD(&__timensResult, &__timensResult, &__timensLwrr.data);         \
    LwU64_ALIGN32_PACK((pTimens), &__timensResult);                            \
} while (LW_FALSE)

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief Structure holding metadata for a parameter RAM buffer used during inference.
 */
struct NNE_DESC_BUFFER
{
    /*!
     * @brief Offset into the parameter RAM that the buffer exists at.
     */
    LwU32 parmRamOffset;
};

/*!
 * @brief DLC RAM state for an NNE_DESC.
 */
struct NNE_DESC_RAM_STATE
{
    /*!
     * @brief RAM state of the buffers used to store intermediate results of the neural-net.
     */
    NNE_DESC_BUFFER                       buffers[NNE_DESC_BUFFER_NUM_BUFFERS];

    /*!
     * @brief Sequence ID, used to determine if this descriptor is resident on
     *        the DLC RAM's.
     */
    LwU32                                 seqId;

    /*!
     * @brief Descriptor RAM offset that the metadata for this NNE_DESC is programmed to.
     */
    LwU32                                 descRamOffset;

    /*!
     * @brief DLC neural-net index that this NNE_DESC has been loaded at.
     *
     * @protected
     */
    LwU8                                  nnetId;
};

/*!
 * @brief Descriptor for a single neural-net.
 *
 * This class represents a single NNE compatible neural-net and conains all data needed to evaluate it.
 *
 * @extends BOARDOBJ
 */
struct NNE_DESC
{
    /*!
     * @brief Base class.
     *
     * Must be first element of the structure to allow casting to parent class.
     *
     * @protected
     */
    BOARDOBJ                              super;

    /*!
     * @brief DLC RAM state for this NNE_DESC.
     *
     * @protected
     */
    NNE_DESC_RAM_STATE                    ramState;

    /*!
     * @brief Index into NNE_LAYERS for the first layer in this neural net.
     *
     * @protected
     */
    LwBoardObjIdx                         firstLayerIdx;

    /*!
     * @brief Index into NNE_LAYERS for the last layer in this neural net.
     *
     * @protected
     */
    LwBoardObjIdx                         lastLayerIdx;

    /*!
     * @brief Index into the variable index array of the first input variable.
     *
     * @protected
     */
    LwU16                                 firstVarIdx;

    /*!
     * @brief Maximum number of outputs from any intermediate (input or hidden)
     *        layer.
     */
    LwU16                                  maxInterLayerNumOutputs;

    /*!
     * @brief Index into the variable index array of the last input variable.
     *
     * @protected
     */
    LwU16                                 lastVarIdx;

    /*!
     * @brief Number of layers in this NNE_DESC.
     */
    LwU16                                 numLayers;

    /*!
     * Version of network which this NNE_DESC object represents.
     *
     * This is a purely diagnostic value to track across potential
     * multiple versions of a network.  This value can be populated in
     * whatever manner the network authors prefer.
     */
    LwU8                                networkVersion;

    /*!
     * @brief Number of outputs this neural-net produces.
     *
     * @ref pDescOutputs
     */
    LwU8                                  numDescOutputs;

    /*!
     * @brief Array describing outputs of this neural-net
     *
     * Dynamically allocated array of LW2080_CTRL_NNE_DESC_OUTPUT
     * ordered in the same way this neural-net will output them.
     *
     * @ref numOutputs
     *
     * @protected
     */
    LW2080_CTRL_NNE_NNE_DESC_OUTPUT_ID   *pDescOutputs;

    /*!
     * @brief Structure holding all relevant data relating to input normalization
     */
    LW2080_CTRL_NNE_NNE_DESC_INPUT_NORM_INFO inputNormInfo;
};

/*!
 * @brief DLC RAM state global to all NNE_DESC
 */
struct NNE_DESCS_RAM_STATE
{
    /*!
     * @brief Sequence ID of the lwrrently loaded NNE_DESC.
     */
    LwU32              seqId;
};

/*!
* @brief Container for all neural-net descriptors
*
* @extends BOARDOBJGRP_E32
*/
struct NNE_DESCS
{
    /*!
     * @brief Base class.
     *
     * Must be first element of the structure to allow casting to parent class.
     *
     * @protected
     */
    BOARDOBJGRP_E32       super;

    /*!
     * @brief DLC RAM state global to all NNE_DESC.
     */
    NNE_DESCS_RAM_STATE   ramState;

    /*!
     * @brief Number of elements in the variable index array. @ref pVarIdxArray.
     */
    LwU16                 numVarIdx;

    /*!
     * @brief Variable index array.
     *
     * Array holding indices into NNE_VARS. NNE_LAYER's reference this array,
     * instead of NNE_VARS directly, so that different neural-nets using the same
     * input variables can reference the same NNE_VAR, rather than instatiating them
     * twice.
     *
     * NOTE: this pointer points to the PMU supersurface because the array is too large
     * to copy into DMEM
     *
     * @protected
     */
    LwBoardObjIdx        *pVarIdxArray;
};

/*!
 * @brief Parse the DLC-binary representation of an output into its corresponding output structure.
 *
 * @param[IN]  pOutputId   Output ID to parse the data as.
 * @param[IN]  data        Binary representation of the output.
 * @param[OUT] pOutput     Output structure to write the parsed data to.
 *
 * @return FLCN_ERR_ILWALID_ARGUMNET   If pOutput is NULL
 * @return FLCN_OK                     If the output binary was successfully parsed.
 */
#define NneDescOutputParse(fname)    FLCN_STATUS (fname)(LW2080_CTRL_NNE_NNE_DESC_OUTPUT_ID *pOutputId, \
    LwF32 data, LW2080_CTRL_NNE_NNE_DESC_OUTPUT *pOutput)

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler    (nneDescBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_nneBoardObj", "nneDescBoardObjGrpIfaceModel10Set");

// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet        (nneDescGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_nneBoardObj", "nneDescGrpIfaceModel10ObjSet_SUPER");

/*!
 * @brief Handler function for any inference requests from PMU clients.
 *
 * @paramp[IN]   pDispatch   Inference message received from NNE clients.
 *
 * @return FLCN_OK                     if the inference message was serviced, irrespective of
 *                                     if an error during inference happened.
 * @return FLCN_ERR_ILWALID_ARGUMENT   if the DMEM overlay of the NNE_DESC_INFERENCE
 *                                     is invalid.
 * @return other                       there was an error sending the completion message
 *                                     back to the client.
 *
 * @memberof NNE_DESC
 */
FLCN_STATUS nneDescInferenceClientHandler(DISPATCH_NNE_NNE_DESC_INFERENCE *pDispatch)
    GCC_ATTRIB_SECTION("imem_nne", "nneDescInferenceClientHandler");

/*!
 * @brief Pre-init setup routine for NNE_DESC
 *
 * @return  FLCN_OK     Pre-init setup was successful
 * @return  Others      Pre-init setup failed
 *
 * @memeberof NNE_DESC
 *
 * @public
 */
FLCN_STATUS nneDescPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "nneDescPreInit");

/*!
 * @brief Post-init setup routine for RPC inference.
 *
 * @return FLCN_OK   Post-init setup was successful
 *
 * @memeberof NNE_DESC
 *
 * @public
 */
FLCN_STATUS nneDescPostInit(void)
    GCC_ATTRIB_SECTION("imem_nneInit", "nneDescInferenceRpcPostInit");

/*!
 * @brief Check if a NNE_DESC has been loaded into DLC RAM's.
 *
 * @param[IN] pDescs   Pointer to global NNE_DESCS.
 * @param[IN] pDesc    NNE_DESC to check for residency.
 *
 * @return LW_TRUE    If @ref pDesc is loaded and ready for invocation.
 * @return LW_FALSE   If @ref pDesc is NOT loaded.
 *
 * @memberof NNE_DESCS
 *
 * @public
 */
LwBool nneDescsDescIsLoaded(NNE_DESCS *pDescs, NNE_DESC *pDesc)
    GCC_ATTRIB_SECTION("imem_nne", "nneDescsDescIsLoaded");

/*!
 * @brief Load a NNE_DESC, and all of its parameters into DLC RAM's.
 *
 * @param[IN] pDescs   Pointer to global NNE_DESCS.
 * @param[IN] pDesc    NNE_DESC to load.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If pDesc is NULL.
 * @return FLCN_ERR_ILWALID_STATE      If a valid layer index without a valid NNE_LAYER object was found.
 * @return other errors                If other internal functions return an error.
 * @return FLCN_OK                     If the NNE_DESC was successfully loaded.
 *
 * @memberof NNE_DESCS
 *
 * @public
 */
FLCN_STATUS nneDescsDescLoad(NNE_DESCS *pDescs, NNE_DESC *pDesc)
    GCC_ATTRIB_SECTION("imem_nne", "nneDescsDescLoad");

/*!
 * @brief Get the number of layers in a neural-net.i
 *
 * @param[IN]  pDesc        Neural-net to get the number of layers for.
 * @param[OUT] pNumLayers   Number of layers in the descriptor.
 *
 * @return
 */
FLCN_STATUS nneDescNumLayersGet(NNE_DESC *pDesc, LwU32 *pNumLayers)
    GCC_ATTRIB_SECTION("imem_nne", "nneDescNumLayersGet");

/*!
 * @brief Get the global sequence ID.
 */
FLCN_STATUS nneDescsSeqIdGet(NNE_DESCS *pDescs, LwU32 *pSeqId)
    GCC_ATTRIB_SECTION("imem_nne", "nneDescsSeqIdGet");

/*!
 * @brief Do inference-specific configuration of the DLC.
 *
 * @param[IN] pDesc                 Descriptor that inference is being run on.
 * @param[IN] loopCnt               Number of inference loops being run.
 * @param[IN] swzlRamOffset         Swizzle RAM offset of input buffer's swizzle.
 * @param[IN] parmRamOutputOffset   Parameter RAM offset of the output buffer.
 * @param[IN] parmRamStatusOffset   Parameter RAM offset of the buffer holding the
 *                                  inference loop status.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If #ref pDesc is NULL.
 * @return FLCN_ERR_ILWALID_STATE      If a valid NNE_LAYER object for the first or
 *                                     last layers of @ref pDesc could not be found.
 */
FLCN_STATUS nneDescInferenceConfig(NNE_DESC *pDesc, LwU8 loopCnt, LwU32 swzlRamOffset,
        LwU32 parmRamOutputOffset, LwU32 parmRamStatusOffset)
    GCC_ATTRIB_SECTION("imem_nne", "nneDescInferenceConfig");

/* ------------------------ Include Derived Types --------------------------- */
#include "nne/nne_desc_fc_10.h"

#endif // NNE_DESC_H
