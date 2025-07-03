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
 * @file    nne_var.h
 * @brief   Neural Net Engine (NNE) variable class interface
 */

#ifndef NNE_VAR_H
#define NNE_VAR_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobj.h"
#include "ctrl/ctrl2080/ctrl2080nne.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct NNE_VAR  NNE_VAR, NNE_VAR_SENSED, NNE_VAR_BASE;

typedef struct NNE_VARS NNE_VARS;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @memberof NNE_VARS
 *
 * @public
 *
 * @brief Retrieve a pointer to the set of all NNE variables (i.e. NNE_VARS)
 *
 * @return  A pointer to the set of all NNE variables casted to NNE_VARS* type
 * @return  NULL if the NNE_VARS PMU features has been disabled
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_NNE_VAR))
#define NNE_VARS_GET()                                                         \
    (&(Nne.vars))
#else
#define NNE_VARS_GET()                                                         \
    ((NNE_VARS *)NULL)
#endif

/*!
 * @memberof NNE_VARS
 *
 * @public
 *
 * @copydoc BOARDOBJGRP_GRP_GET()
 */
#define BOARDOBJGRP_DATA_LOCATION_NNE_VAR                                      \
    (&(NNE_VARS_GET()->super.super))

/*!
 * @memberof NNE_VARS
 *
 * @pubic
 *
 * @brief Retrieve a pointer to a particular NNE_VAR in the set of all NNE_VARS
 * by index.
 *
 * @param[in]  _idx    Index into NNE_VARS of requested NNE_VAR object
 *
 * @return  A pointer to the NNE_VAR at the requested index.
 */
#define NNE_VAR_GET(_idx)                                                      \
    ((NNE_VAR *)BOARDOBJGRP_OBJ_GET(NNE_VAR, (_idx)))

/*!
 * @member of NNE_VARS
 *
 * @pubic
 *
 * @brief Accessor macro to retieve a pointer to the @ref
 * NNE_VARS_VAR_ID_MAP structure within @ref NNE_VARS.
 *
 * @param[in]  _pVars    NNE_VARS pointer.
 *
 * @return  A pointer to the @ref NNE_VARS_VAR_ID_MAP structure.
*/
#define NNE_VARS_VAR_ID_MAP_GET(_pVars)                                         \
    (((_pVars) != NULL) ? &((_pVars)->varIdMap) : (NNE_VARS_VAR_ID_MAP *)NULL)

/*!
 * @member of NNE_VARS_VAR_ID_MAP
 *
 * @public
 *
 * @defgroup NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX
 *
 * Macros defining the maximum number of entries in the @ref
 * NNE_VARS_VAR_ID_MAP array for a given @ref
 * LW2080_CTRL_NNE_NNE_VAR_TYPE_ENUM.
 *
 * These values must be maintained in sync with type-specific
 * implementations of @ref NneVarVarIdMapIdxGet() for how those
 * implementations colwert the LW2080_CTRL_NNE_NNE_VAR_ID_<XYZ> into a
 * MAP_IDX.
 *
 * @{
 */
#define NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX(_type)                             \
    NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_##_type
#define NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_FREQ                               6
#define NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_PM                               390
#define NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_CHIP_CONFIG                        3
#define NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_POWER_DN                           2
#define NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX_POWER_TOTAL                        1
/*! }@ */

/*!
 * @member of NNE_VARS_VAR_ID_MAP
 *
 * @public
 *
 * @defgroup NNE_VARS_VAR_ID_MAP_TYPE_ARR_NAME
 *
 * Macros defining the name of the @ref NNE_VARS_VAR_ID_MAP array for
 * a given @ref LW2080_CTRL_NNE_NNE_VAR_TYPE_ENUM.
 *
 * @{
 */
#define NNE_VARS_VAR_ID_MAP_TYPE_ARR_NAME(_type)                                \
    NNE_VARS_VAR_ID_MAP_TYPE_NAME_##_type
#define NNE_VARS_VAR_ID_MAP_TYPE_NAME_FREQ                                   freq
#define NNE_VARS_VAR_ID_MAP_TYPE_NAME_PM                                       pm
#define NNE_VARS_VAR_ID_MAP_TYPE_NAME_CHIP_CONFIG                      chipConfig
#define NNE_VARS_VAR_ID_MAP_TYPE_NAME_POWER_DN                            powerDN
#define NNE_VARS_VAR_ID_MAP_TYPE_NAME_POWER_TOTA                       powerTotal
/*! }@ */

/*!
 * @member of NNE_VARS_VAR_ID_MAP
 *
 * @public
 *
 * @brief Macro to declare a @ref NNE_VARS_VAR_ID_MAP array for a
 * given @ref LW2080_CTRL_NNE_NNE_VAR_TYPE_ENUM.
 */
#define NNE_VARS_VAR_ID_MAP_TYPE_ARR_DECLARE(_type)                                    \
    LwBoardObjIdx NNE_VARS_VAR_ID_MAP_TYPE_ARR_NAME(_type)[NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX(_type)]

/*!
 * @member of NNE_VARS_VAR_ID_MAP
 *
 * @public
 *
 * @brief Accessor macro to retrieve the @ref NNE_VAR index output of an index
 * within a @ref NNE_VARS_VAR_ID_MAP array for a given @ref
 * LW2080_CTRL_NNE_NNE_VAR_TYPE_ENUM.
 *
 * @param[out] _status
 *     FLCN_STATUS variable name in which to return the status of the
 *     accessor.  Will be set to @ref FLCN_OK if retrieved
 *     successfully, @ref FLCN_ERR_ILWALID_ARGUMENT if the specified
 *     array index is out of bounds.
 * @param[in]  _pVarIdMap
 *     @ref NNE_VARS_VAR_ID_MAP pointer.
 * @param[in]  _type
 *     Type string specifying the @ref NNE_VARS_VAR_ID_MAP array to reference.
 * @param[in]  _mapArrIdx
 *     Index within the the @ref NNE_VARS_VAR_ID_MAP array to retreive.
 * @param[out] _pVarIdx
 *     Pointer in which to return the requested @ref NNE_VAR index mapping.
 * @param[in]  _label
 *     Label string to use to jump to on any failures.
 */
#define NNE_VARS_VAR_ID_MAP_TYPE_ARR_GET_IDX(_status, _pVarIdMap, _type, _mapArrIdx, _pVarIdx, _label) \
do {                                                                            \
    PMU_ASSERT_OK_OR_GOTO(_status,                                              \
        ((((_pVarIdMap != NULL) &&                                              \
                (_mapArrIdx) < NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX(_type))) ?  \
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),                               \
        _label);                                                                \
    (*_pVarIdx) =                                                               \
        (_pVarIdMap)->NNE_VARS_VAR_ID_MAP_TYPE_ARR_NAME(_type)[(_mapArrIdx)];   \
} while (LW_FALSE)

/*!
 * @member of NNE_VARS_VAR_ID_MAP
 *
 * @public
 *
 * @brief Mutator macro to set the @ref NNE_VAR index output of an index
 * within a @ref NNE_VARS_VAR_ID_MAP array for a given @ref
 * LW2080_CTRL_NNE_NNE_VAR_TYPE_ENUM.
 *
 * @param[out] _status
 *     FLCN_STATUS variable name in which to return the status of the
 *     mutator.  Will be set to @ref FLCN_OK if set successfully, @ref
 *     FLCN_ERR_ILWALID_ARGUMENT if the specified array index is out
 *     of bounds, @ref FLCNERR_ILWALID_STATE if the specified index is
 *     already set.
 * @param[in]  _pVarIdMap
 *     @ref NNE_VARS_VAR_ID_MAP pointer.
 * @param[in]  _type
 *     Type string specifying the @ref NNE_VARS_VAR_ID_MAP array to reference.
 * @param[in]  _mapArrIdx
 *     Index within the the @ref NNE_VARS_VAR_ID_MAP array to retreive.
 * @param[in]  _varIdx
 *     Value to set in the requested @ref NNE_VAR index mapping.
 * @param[in]  _label
 *     Label string to use to jump to on any failures.
 */
#define NNE_VARS_VAR_ID_MAP_TYPE_ARR_SET_IDX(_status, _pVarIdMap, _type, _mapArrIdx, _varIdx, _label) \
do {                                                                            \
    LwBoardObjIdx _varIdxLwrr;                                                  \
                                                                                \
    PMU_ASSERT_OK_OR_GOTO(_status,                                              \
        ((_pVarIdMap != NULL) ?                                                 \
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),                               \
        _label);                                                                \
                                                                                \
    NNE_VARS_VAR_ID_MAP_TYPE_ARR_GET_IDX(_status, (_pVarIdMap), _type,          \
        (_mapArrIdx), &_varIdxLwrr, _label);                                    \
    PMU_ASSERT_OK_OR_GOTO(_status,                                              \
        ((_varIdxLwrr == LW2080_CTRL_BOARDOBJ_IDX_ILWALID) ?                    \
            FLCN_OK : FLCN_ERR_ILWALID_STATE),                                  \
        _label);                                                                \
                                                                                \
    (_pVarIdMap)->NNE_VARS_VAR_ID_MAP_TYPE_ARR_NAME(_type)[(_mapArrIdx)] =      \
        (_varIdx);                                                              \
} while (LW_FALSE)


/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief Base class for all input variables to the NNE
 *
 * NNE_VAR is a virtual base class that represents a single input
 * signal to a NNE-compliant neural-net. A neural-net will have one
 * NNE_VAR for every individual ingput signal.
 *
 * @extends BOARDOBJ
 */
struct NNE_VAR
{
    /*!
     * @brief Base class.
     *
     * Must be first element of the structure to allow casting to parent class.
     *
     * @protected
     */
    BOARDOBJ    super;
};

/*!
 * Map of LW2080_CTRL_NNE_NNE_VAR_ID_<XYZ> to NNE_VAR idx.
 *
 * This structure is used to implement O(1) lookup from
 * LW2080_CTRL_NNE_NNE_VAR_ID_<XYZ> (used in @ref
 * LW2080_CTRL_NNE_NNE_VAR_INPUT within client input) to the
 * correspodning NNE_VAR idx.
 */
typedef struct
{
    /*!
     * Map array for type @ref LW2080_CTRL_NNE_NNE_VAR_TYPE_FREQ
     */
    NNE_VARS_VAR_ID_MAP_TYPE_ARR_DECLARE(FREQ);
    /*!
     * Map array for type @ref LW2080_CTRL_NNE_NNE_VAR_TYPE_PM
     */
    NNE_VARS_VAR_ID_MAP_TYPE_ARR_DECLARE(PM);
    /*!
     * Map array for type @ref LW2080_CTRL_NNE_NNE_VAR_TYPE_CHIP_CONFIG
     */
    NNE_VARS_VAR_ID_MAP_TYPE_ARR_DECLARE(CHIP_CONFIG);
    /*!
     * Map array for type @ref LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_DN
     */
    NNE_VARS_VAR_ID_MAP_TYPE_ARR_DECLARE(POWER_DN);
    /*!
     * Map array for type @ref LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_TOTAL
     */
    NNE_VARS_VAR_ID_MAP_TYPE_ARR_DECLARE(POWER_TOTAL);
} NNE_VARS_VAR_ID_MAP;

/*!
 * @brief Container for NNE_VAR
 *
 * @extends BOARDOBJGRP_E512
 */
struct NNE_VARS
{
    /*!
     * @brief Base class.
     *
     * Must be first element of the structure to allow casting to parent class.
     * @protected
     */
    BOARDOBJGRP_E512 super;

    /*!
     * Map of LW2080_CTRL_NNE_NNE_VAR_ID_<XYZ> to NNE_VAR idx.
     */
    NNE_VARS_VAR_ID_MAP varIdMap;
};

/*!
 * @brief Determine if an NNE_VAR and NNE_VAR_INPUT refer to the same input signal.
 *
 * @param[IN]  pVar     NNE_VAR ID to compare against.
 * @param[IN]  pInput   Signal input to check the ID of.
 * @param[OUT] pMatch   Boolean showing if IDs @ref pVar and @ref pInput match.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT  If @ref pVar, @ref pInput, or @ref pMatch are NULL
 * @return FLCN_ERR_ILWALID_STATE     If an unsupported variable type was detected.
 * @return FLCN_OK                    If a match of IDs was successfully determined.
 */
#define NneVarInputIDMatch(fname)   FLCN_STATUS (fname)(NNE_VAR *pVar, LW2080_CTRL_NNE_NNE_VAR_INPUT *pInput, LwBool * pMatch)

/*!
 * @member of NNE_VAR
 *
 * @private
 *
 * @brief Interface which generates the corresponding index within a
 * @ref NNE_VARS_VAR_ID_MAP array for a @ref
 * LW2080_CTRL_NNE_NNE_VAR_INPUT structure for a given @ref
 * LW2080_CTRL_NNE_NNE_VAR_TYPE_ENUM class.
 *
 * This is essentially a "hash" function - creating a unique index
 * from the corresponding LW2080_CTRL_NNE_VAR_ID_<TYPE> structure.
 * The semantics of this creating this index is entirely
 * class-specific and must be kept always return a value less than the
 * given @ref LW2080_CTRL_NNE_NNE_VAR_TYPE_ENUM class's @ref
 * NNE_VARS_VAR_ID_MAP_TYPE_ARR_IDX_MAX value.
 *
 * @param[in]  pInput     @ref LW2080_CTRL_NNE_NNE_VAR_INPUT pointer
 * @param[out] pMapArrIdx Pointer in which to return the generate array index.
 *
 * @return FLCN_OK
 *     Array index value generated successfully.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Invalid @ref pInput.
 */
#define NneVarsVarIdMapArrIdxGet(fname)   FLCN_STATUS (fname)(LW2080_CTRL_NNE_NNE_VAR_INPUT *pInput, LwU16 *pMapArrIdx)

/*!
 * @brief Get the LwF32 representation for this variable.
 *
 * TODO: Augment to take in an NNE_VAR that the input should match. This is to
 *       further protect against non-sensical inputs being passed in
 *
 * @param[IN]  pInput      The input to get the LwF32 representation for.
 * @param[OUT] pVarValue   The value of the input variable in F32 representation.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pInput or @ref pVarValue is NULL.
 * @return FLCN_ERR_ILWALID_STATE      If an invalid input type was detected.
 * @return FLCN_OK                     If the F33 representation of the value was
 *                                     successfully retrieved.
 */
#define NneVarLwF32Get(fname) FLCN_STATUS (fname)(const LW2080_CTRL_NNE_NNE_VAR_INPUT *pInput, LwF32 * pVarValue)

/*!
 * @brief   Gets the normalization ratio for this input
 *
 * @param[in]   pVar            @ref NNE_VAR for this input
 * @param[in]   pInput          Input to normalize
 * @param[out]  pNormalization  Normalization ratio
 *
 * @return  FLCN_OK Success
 * @return  Others  Implementation-specific errors.
 */
#define NneVarInputNormalizationGet(fname) FLCN_STATUS (fname)(const NNE_VAR *pVar, const LW2080_CTRL_NNE_NNE_VAR_INPUT *pInput, LwF32 *pNormalization)

/*!
 * @brief   Normalizes the input using given normalization ratio, if appropriate
 *          for the type of input.
 *
 * @param[in,out]   pInput  Input to normalize
 * @param[in]       norm    Ratio for normalization
 *
 * @return  FLCN_OK Success
 * @return  Others  Implementation-specific errors.
 */
#define NneVarInputNormalize(fname) FLCN_STATUS (fname)(LW2080_CTRL_NNE_NNE_VAR_INPUT *pInput, LwF32 norm)

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler   (nneVarBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_nneBoardObj", "nneVarBoardObjGrpIfaceModel10Set");

// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet      (nneVarGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_nneBoardObj", "nneVarGrpIfaceModel10ObjSet_SUPER");

/*!
 * @member of NNE_VARS
 *
 * @public

 * @brief Post-init of NNE_VARS object.
 *
 * @return FLCN_OK   Post-init setup was successful
 */
FLCN_STATUS nneVarsPostInit(void)
    GCC_ATTRIB_SECTION("imem_nneInit", "nneVarsPostInit");

/*!
 * @copydoc NneVarInputIDMatch
 */
NneVarInputIDMatch (nneVarInputIDMatch)
    GCC_ATTRIB_SECTION("imem_nne", "nneVarInputIDMatch");

/*!
 * @copydoc NneVarLwF32Get
 */
NneVarLwF32Get  (nneVarLwF32Get)
    GCC_ATTRIB_SECTION("imem_nne", "nneVarLwF32Get");

/*!
 * @copydoc NneVarInputNormalizationGet
 */
NneVarInputNormalizationGet (nneVarInputNormalizationGet)
    GCC_ATTRIB_SECTION("imem_nneInferenceClient", "nneVarInputNormalizationGet");

/*!
 * @copydoc NneVarInputNormalize
 */
NneVarInputNormalize (nneVarInputNormalize)
    GCC_ATTRIB_SECTION("imem_nneInferenceClient", "nneVarInputNormalize");

/*!
 * @member of NNE_VARS
 *
 * @public
 *
 * @brief Looks up the corresonding @ref NNE_VAR index for a @ref
 * LW2080_CTRL_NNE_NNE_VAR_INPUT structure, per its ID information,
 * within a @ref NNE_VARS_VAR_ID_MAP.
 *
 * If the specified @ref LW2080_CTRL_NNE_NNE_VAR_INPUT structure does
 * not exist in the NNE_VARS group, it will return @ref
 * LW2080_CTRL_BOARDOBJ_IDX_ILWALID.
 *
 * @param[in]   pVarIdMap
 *     @ref NNE_VARS_VAR_ID_MAP pointer.
 * @param[in]   pInput
 *     @ref LW2080_CTRL_NNE_NNE_VAR_INPUT pointer
 * @param[out]  pVarIdx
 *     Pointer in which to return the corresponding @ref NNE_VAR index.
 *
 * @return FLCN_OK
 *     Look up completed succesfully.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Invalid @ref pInput.
 */
FLCN_STATUS nneVarsVarIdMapInputToVarIdx(NNE_VARS_VAR_ID_MAP *pVarIdMap, LW2080_CTRL_NNE_NNE_VAR_INPUT *pInput, LwBoardObjIdx *pVarIdx)
    GCC_ATTRIB_SECTION("imem_nne", "nneVarsVarIdMapInputToVarIdx");

/*!
 * @brief   Applies normalization to the @ref LW2080_CTRL_NNE_NNE_VAR_INPUT
 *          values.
 *
 * @details Neural nets can be ineffective outside of their trained range. This
 *          function applies normalization, when necessary, to bound the input
 *          values to the network.
 *
 * @param[in,out]   pInputs           Inputs to normalize
 * @param[in]       numInputs         Number of inputs
 * @param[in]       descIdx           Index of a NNE_DESC object
 * @param[in]       pInputNormStatus  Pointer to structure holding the _INPUT_NORM_STATUS
 * @param[out]      pNormRatio        The ratio used to normalize the inputs. Clients
 *                                    may need this to contextualize outputs of the
 *                                    network.
 *
 * @return  FLCN_OK                     Success
 * @return  FLCN_ERR_ILWALID_ARGUMENT   pInputs or pNormRatio is NULL or
 *                                      an element of pInputs could not be
 *                                      matched with an @ref NNE_VAR.
 * @return  Others                      Errors propagated from callees.
 */
FLCN_STATUS nneVarsVarInputsNormalize(LW2080_CTRL_NNE_NNE_VAR_INPUT *pInputs, LwU32 numInputs, LwBoardObjIdx descIdx,
                LW2080_CTRL_NNE_NNE_DESC_INPUT_NORM_STATUS *pInputNormStatus, LwF32 *pNormRatio)
    GCC_ATTRIB_SECTION("imem_nneInferenceClient", "nneVarsVarInputsNormalize");

/*!
 * @brief Write all inference input values to parameter RAM.
 *
 * @param[IN] pInputs         Array of inputs to write to parameter RAM, in order.
 * @param{IN] parmRamOffset   Offset into the parameter RAM to start writing the input values.
 * @param[IN] numInputs       Number of inputs to write to parameter RAM.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pInputs is NULLi
 * @return Other errors                If a function called by this one returns an error.
 * @return FLCN_OK                     If the array of inputs was successfully writing to parameter RAM.
 */
FLCN_STATUS nneVarsVarInputsWrite(LW2080_CTRL_NNE_NNE_VAR_INPUT *pInputs, LwU32 parmRamOffset, LwU32 numInputs)
    GCC_ATTRIB_SECTION("imem_nne", "nneVarsVarInputsWrite");

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief   NneVarInputNormalizationGet
 */
static inline FLCN_STATUS
nneVarInputNormalizationGet_SUPER
(
    const NNE_VAR *pVar,
    const LW2080_CTRL_NNE_NNE_VAR_INPUT *pInput,
    LwF32 *pNormalization
)
{
    *pNormalization = 1.0F;
    return FLCN_OK;
}

/*!
 * @brief   NneVarInputNormalize
 */
static inline FLCN_STATUS
nneVarInputNormalize_SUPER
(
    LW2080_CTRL_NNE_NNE_VAR_INPUT *pInput,
    LwF32 norm
)
{
    return FLCN_OK;
}

/* ------------------------ Include Derived Types --------------------------- */
#include "nne/nne_var_chip_config.h"
#include "nne/nne_var_freq.h"
#include "nne/nne_var_pm.h"
#include "nne/nne_var_power_dn.h"
#include "nne/nne_var_power_total.h"

#endif // NNE_VAR_H
