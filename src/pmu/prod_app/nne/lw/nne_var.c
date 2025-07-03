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
 * @file   nne_var.c
 * @brief  Neural Net Engine (NNE) variable methods.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "objnne.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjGrpIfaceModel10SetHeader              (s_nneVarIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_nneBoardObj", "s_nneVarIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry               (s_nneVarIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_nneBoardObj", "s_nneVarIfaceModel10SetEntry");

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */

//
// Number of DWORDS that should be cached in DMEM prior to be written to the
// parameter RAM.
//
#define PARM_RAM_STAGING_DWORDS                                              (4)

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief Initializes the @ref NNE_VARS_VAR_ID_MAP structure.
 *
 * @param[in/out] pVarIdMap
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
nneVarsVarIdMapInit
(
    NNE_VARS_VAR_ID_MAP *pVarIdMap
)
{
    memset(pVarIdMap, LW_U8_MAX, sizeof(*pVarIdMap));
}

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief NNE_VARS implementation of BoardObjGrpIfaceModel10CmdHandler().
 *
 * @memberof NNE_VARS
 *
 * @public
 *
 * @copydetails BoardObjGrpIfaceModel10CmdHandler()
 */
FLCN_STATUS
nneVarBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status = FLCN_OK;

    PMU_ASSERT_OK_OR_GOTO(status,
        BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E512,            // _grpType
            NNE_VAR,                                    // _class
            pBuffer,                                    // _pBuffer
            s_nneVarIfaceModel10SetHeader,                    // _hdrFunc
            s_nneVarIfaceModel10SetEntry,                     // _entryFunc
            ga10xAndLater.nne.nneVarGrpSet,             // _ssElement
            OVL_INDEX_DMEM(nne),                        // _ovlIdxDmem
            BoardObjGrpVirtualTablesNotImplemented),    // _ppObjectVtables
        nneVarBoardObjGrpIfaceModel10Set_exit);

nneVarBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * @copydoc nneVarsPostInit
 */
FLCN_STATUS
nneVarsPostInit(void)
{
    NNE_VARS   *pVars = NNE_VARS_GET();
    FLCN_STATUS status = FLCN_OK;

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pVars != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        nneVarsPostInit_exit);

    //
    // Init the NNE_VARS_VAR_ID_MAP to all unset.  Individual controllers will add
    // themselves as they construct.
    //
    nneVarsVarIdMapInit(NNE_VARS_VAR_ID_MAP_GET(pVars));

nneVarsPostInit_exit:
    return status;
}

/*!
 * @copydoc NneVarLwF32Get
 */
FLCN_STATUS
nneVarLwF32Get
(
    const LW2080_CTRL_NNE_NNE_VAR_INPUT    *pInput,
    LwF32                                  *pVarValue
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if ((pInput    == NULL) ||
        (pVarValue == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneVarLwF32Get_exit;
    }

    switch (pInput->type)
    {
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_FREQ:
            status = nneVarLwF32Get_FREQ(pInput, pVarValue);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_PM:
            status = nneVarLwF32Get_PM(pInput, pVarValue);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_CHIP_CONFIG:
            status = nneVarLwF32Get_CHIP_CONFIG(pInput, pVarValue);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_DN:
            status = nneVarLwF32Get_POWER_DN(pInput, pVarValue);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_TOTAL:
            status = nneVarLwF32Get_POWER_TOTAL(pInput, pVarValue);
            break;
        default:
            PMU_TRUE_BP();
            status = FLCN_ERR_ILWALID_STATE;
            goto nneVarLwF32Get_exit;
            break;
    }

nneVarLwF32Get_exit:
    return status;
}

/*!
 * @copydoc NneVarInputNormalizationGet
 */
FLCN_STATUS
nneVarInputNormalizationGet
(
    const NNE_VAR *pVar,
    const LW2080_CTRL_NNE_NNE_VAR_INPUT *pInput,
    LwF32 *pNormalization
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        (pVar != NULL) &&
        (pInput != NULL) &&
        (BOARDOBJ_GET_TYPE(&pVar->super) == pInput->type) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        nneVarInputNormalizationGet_exit);

    switch (BOARDOBJ_GET_TYPE(&pVar->super))
    {
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_FREQ:
            status = nneVarInputNormalizationGet_FREQ(pVar, pInput, pNormalization);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_PM:
            status = nneVarInputNormalizationGet_PM(pVar, pInput, pNormalization);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_CHIP_CONFIG:
            status = nneVarInputNormalizationGet_CHIP_CONFIG(pVar, pInput, pNormalization);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_DN:
            status = nneVarInputNormalizationGet_POWER_DN(pVar, pInput, pNormalization);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_TOTAL:
            status = nneVarInputNormalizationGet_POWER_TOTAL(pVar, pInput, pNormalization);
            break;
        default:
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
    }

    if (status != FLCN_OK)
    {
        PMU_TRUE_BP();
        goto nneVarInputNormalizationGet_exit;
    }

nneVarInputNormalizationGet_exit:
    return status;
}

/*!
 * @copydoc NneVarInputNormalize
 */
FLCN_STATUS
nneVarInputNormalize
(
    LW2080_CTRL_NNE_NNE_VAR_INPUT *pInput,
    LwF32 norm
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        pInput != NULL ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        nneVarInputNormalize_exit);

    switch (pInput->type)
    {
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_FREQ:
            status = nneVarInputNormalize_FREQ(pInput, norm);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_PM:
            status = nneVarInputNormalize_PM(pInput, norm);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_CHIP_CONFIG:
            status = nneVarInputNormalize_CHIP_CONFIG(pInput, norm);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_DN:
            status = nneVarInputNormalize_POWER_DN(pInput, norm);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_TOTAL:
            status = nneVarInputNormalize_POWER_TOTAL(pInput, norm);
            break;
        default:
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
    }

    if (status != FLCN_OK)
    {
        PMU_TRUE_BP();
        goto nneVarInputNormalize_exit;
    }

nneVarInputNormalize_exit:
    return status;
}

/*!
 * @copydoc NneVarInputMapArrIdxGet
 */
FLCN_STATUS
nneVarsVarIdMapInputToVarIdx
(
    NNE_VARS_VAR_ID_MAP            *pVarIdMap,
    LW2080_CTRL_NNE_NNE_VAR_INPUT  *pInput,
    LwBoardObjIdx                  *pVarIdx
)
{
    FLCN_STATUS status;
    LwU16       mapArrIdx;

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pVarIdx != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
        nneVarsVarIdMapInputToVarIdx_exit);

    switch (pInput->type)
    {
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_FREQ:
            PMU_ASSERT_OK_OR_GOTO(status,
                nneVarsVarIdMapArrIdxGet_FREQ(pInput, &mapArrIdx),
                nneVarsVarIdMapInputToVarIdx_exit);
            NNE_VARS_VAR_ID_MAP_TYPE_ARR_GET_IDX(status, pVarIdMap, FREQ,
                mapArrIdx, pVarIdx, nneVarsVarIdMapInputToVarIdx_exit);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_PM:
            PMU_ASSERT_OK_OR_GOTO(status,
                nneVarsVarIdMapArrIdxGet_PM(pInput, &mapArrIdx),
                nneVarsVarIdMapInputToVarIdx_exit);
            NNE_VARS_VAR_ID_MAP_TYPE_ARR_GET_IDX(status, pVarIdMap, PM,
                mapArrIdx, pVarIdx, nneVarsVarIdMapInputToVarIdx_exit);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_CHIP_CONFIG:
            PMU_ASSERT_OK_OR_GOTO(status,
                nneVarsVarIdMapArrIdxGet_CHIP_CONFIG(pInput, &mapArrIdx),
                nneVarsVarIdMapInputToVarIdx_exit);
            NNE_VARS_VAR_ID_MAP_TYPE_ARR_GET_IDX(status, pVarIdMap, CHIP_CONFIG,
                mapArrIdx, pVarIdx, nneVarsVarIdMapInputToVarIdx_exit);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_DN:
            PMU_ASSERT_OK_OR_GOTO(status,
                nneVarsVarIdMapArrIdxGet_POWER_DN(pInput, &mapArrIdx),
                nneVarsVarIdMapInputToVarIdx_exit);
            NNE_VARS_VAR_ID_MAP_TYPE_ARR_GET_IDX(status, pVarIdMap, POWER_DN,
                mapArrIdx, pVarIdx, nneVarsVarIdMapInputToVarIdx_exit);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_TOTAL:
            PMU_ASSERT_OK_OR_GOTO(status,
                nneVarsVarIdMapArrIdxGet_POWER_TOTAL(pInput, &mapArrIdx),
                nneVarsVarIdMapInputToVarIdx_exit);
            NNE_VARS_VAR_ID_MAP_TYPE_ARR_GET_IDX(status, pVarIdMap, POWER_TOTAL,
                mapArrIdx, pVarIdx, nneVarsVarIdMapInputToVarIdx_exit);
            break;
        default:
            PMU_TRUE_BP();
            status = FLCN_ERR_ILWALID_STATE;
            *pVarIdx = LW2080_CTRL_BOARDOBJ_IDX_ILWALID;
            goto nneVarsVarIdMapInputToVarIdx_exit;
            break;
    }

nneVarsVarIdMapInputToVarIdx_exit:
    return status;
}

/*!
 * @copydoc nneVarsVarInputsNormalize
 */
FLCN_STATUS
nneVarsVarInputsNormalize
(
    LW2080_CTRL_NNE_NNE_VAR_INPUT *pInputs,
    LwU32 numInputs,
    LwBoardObjIdx descIdx,
    LW2080_CTRL_NNE_NNE_DESC_INPUT_NORM_STATUS *pInputNormStatus,
    LwF32 *pNormRatio
)
{
    FLCN_STATUS status;
    LwU32 i;
    LwF32 maxNorm;
    LwU16 inputThresholdCount = 0;
    LW2080_CTRL_NNE_NNE_DESC_INPUT_NORM_INFO *pInputNormInfo;
    NNE_DESC *pDesc = NNE_DESC_GET(descIdx);

    // Sanity Checks
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pInputs != NULL) && (pNormRatio != NULL) &&
        (pInputNormStatus != NULL) && (pDesc != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        nneVarsVarInputsNormalize_exit);

    pInputNormInfo = &(pDesc->inputNormInfo);

    // Reset the status structure
    LW2080_CTRL_NNE_NNE_DESC_INPUT_NORM_STATUS_RESET(pInputNormStatus);

    maxNorm = 1.0F;
    for (i = 0; i < numInputs; i++)
    {
        LwBoardObjIdx varIdx;
        const NNE_VAR *pVar;
        LwF32 norm;

        PMU_ASSERT_OK_OR_GOTO(status,
            nneVarsVarIdMapInputToVarIdx(
                NNE_VARS_VAR_ID_MAP_GET(NNE_VARS_GET()),
                &pInputs[i],
                &varIdx),
            nneVarsVarInputsNormalize_exit);

        //
        // If the map returns an invalid index, the neural-network
        // does not use this input. Skip it.
        //
        if (varIdx == LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
        {
            continue;
        }

        PMU_ASSERT_OK_OR_GOTO(status,
            (pVar = NNE_VAR_GET(varIdx)) != NULL ?
                FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
            nneVarsVarInputsNormalize_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            nneVarInputNormalizationGet(pVar, &pInputs[i], &norm),
            nneVarsVarInputsNormalize_exit);

        //
        // If the norm is greater then 1.0, the the specific input
        // has violated it's normValue so increment the counts of total
        // violations and add it to a mask of violating inputs
        //
        if (lwF32CmpGT(norm, 1.0F))
        {
            pInputNormStatus->violationCount++;
            LW2080_CTRL_BOARDOBJGRP_MASK_BIT_SET(&(pInputNormStatus->violatiolwarMask.super),
                varIdx);

            // Update maxNorm to be norm if norm > prevMaxNorm
            if (lwF32CmpGT(norm, maxNorm))
            {
                maxNorm = norm;
            }
        }
    }

    //
    // If any of the values are out of the expected range, then we must
    // normalize based on that ratio.
    //
    if (lwF32CmpGT(maxNorm, 1.0F))
    {
        //
        // Search through inputNormThresholds and find the closest threshold
        // to violationCount that is still below the violationCount
        // value. Update appliedMode to be the index at which we find this
        // result.
        //
        for (i = 0; i < LW2080_CTRL_NNE_NNE_DESC_INPUT_NORM_MODE_NUM_MODES; i++)
        {
            if (pInputNormStatus->violationCount >= pInputNormInfo->modeThresholds[i] &&
                pInputNormInfo->modeThresholds[i] > inputThresholdCount)
            {
                inputThresholdCount = pInputNormInfo->modeThresholds[i];
                pInputNormStatus->appliedMode = i;
            }
        }

        // If the appliedMode is _POISON, exit early.
        if (pInputNormStatus->appliedMode == LW2080_CTRL_NNE_NNE_DESC_INPUT_NORM_MODE_POISON)
        {
            goto nneVarsVarInputsNormalize_exit;
        }

        // Apply normalization
        for (i = 0; i < numInputs; i++)
        {
            LwBoardObjIdx varIdx;

            // Map the input number to the varIdx we are interested in
            PMU_ASSERT_OK_OR_GOTO(status,
                nneVarsVarIdMapInputToVarIdx(
                    NNE_VARS_VAR_ID_MAP_GET(NNE_VARS_GET()),
                    &pInputs[i],
                    &varIdx),
                nneVarsVarInputsNormalize_exit);

            //
            // If the map returns an invalid index, the neural-network
            // does not use this input. Skip it.
            //
            if (varIdx == LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
            {
                continue;
            }

            //
            // If the treshold closest to violationCount is _SATURATED and the input's varIdx is in
            // the violatiolwarMask, self-normalize the input
            //
            if (pInputNormStatus->appliedMode == LW2080_CTRL_NNE_NNE_DESC_INPUT_NORM_MODE_SATURATION &&
                LW2080_CTRL_BOARDOBJGRP_MASK_BIT_GET(&(pInputNormStatus->violatiolwarMask.super),
                    varIdx))
            {
                const NNE_VAR *pVar;
                LwF32 norm;

                // Get the NNE_VAR object for the variable
                PMU_ASSERT_TRUE_OR_GOTO(status,
                    (pVar = NNE_VAR_GET(varIdx)) != NULL,
                    FLCN_ERR_ILWALID_ARGUMENT,
                    nneVarsVarInputsNormalize_exit);

                PMU_ASSERT_OK_OR_GOTO(status,
                    nneVarInputNormalizationGet(pVar, &pInputs[i], &norm),
                    nneVarsVarInputsNormalize_exit);

                PMU_ASSERT_OK_OR_GOTO(status,
                    nneVarInputNormalize(&pInputs[i], norm),
                    nneVarsVarInputsNormalize_exit);
            }
            //
            // Else if the treshold closest to violationCount is _SECONDARY_NORM, normalize
            // all inputs by maxNorm
            //
            else if (pInputNormStatus->appliedMode == LW2080_CTRL_NNE_NNE_DESC_INPUT_NORM_MODE_SECONDARY)
            {
                PMU_ASSERT_OK_OR_GOTO(status,
                    nneVarInputNormalize(&pInputs[i], maxNorm),
                    nneVarsVarInputsNormalize_exit);
            }
            else
            {
                // Not an error
            }
        }
    }

    *pNormRatio = maxNorm;

nneVarsVarInputsNormalize_exit:
    return status;
}

/*!
 * @copydoc nneVarsVarInputsWrite
 */
FLCN_STATUS
nneVarsVarInputsWrite
(
    LW2080_CTRL_NNE_NNE_VAR_INPUT   *pInputs,
    LwU32                            parmRamOffset,
    LwU32                            numInputs
)
{
    LwU32         idx;
    LwU32         stagingIdx;
    LwU32         ramWriteBase;
    LwU32         inputsStaging[PARM_RAM_STAGING_DWORDS];
    LwF32         varF32;
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    if (pInputs == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneVarsVarInputsWrite_exit;
    }

    // Copy the inputs in chunks.
    ramWriteBase = parmRamOffset;
    for (idx = 0; idx < numInputs; idx++)
    {
        stagingIdx = idx % PARM_RAM_STAGING_DWORDS;

        PMU_ASSERT_OK_OR_GOTO(status,
            nneVarLwF32Get(&(pInputs[idx]), &varF32),
            nneVarsVarInputsWrite_exit);

        inputsStaging[stagingIdx] = lwF32MapToU32(&varF32);

        //
        // When the staging buffer is full, or if this is the last input to write to
        // parameter RAM, write the contents in the staging buffer to the parameter RAM.
        //
        if ((stagingIdx == (PARM_RAM_STAGING_DWORDS - 1)) ||
            (idx        == (numInputs - 1)))
        {
            //
            // The number of dwords to be written to disk is the remaining amount
            // of words that needs to be written, capped to the number of dwords the
            // staging buffer holds. The number of bytes is then that number
            // times 4.
            //
            const LwU32 ramWriteNumBytes =
                (stagingIdx + 1) * sizeof(LwU32);

            PMU_ASSERT_OK_OR_GOTO(status,
                nneParmRamAccess_HAL(&Nne,
                                     inputsStaging,
                                     ramWriteBase,
                                     ramWriteNumBytes,
                                     LW_TRUE),
                nneVarsVarInputsWrite_exit);

            // Increment where the next write into the parameter RAM should be to.
            ramWriteBase += ramWriteNumBytes;
        }
    }

nneVarsVarInputsWrite_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10SetHeader()
 *
 * @memberof NNE_VARS
 *
 * @private
 */
static FLCN_STATUS
s_nneVarIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    NNE_VARS   *pVars = (NNE_VARS *)pBObjGrp;
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pVars != NULL)  ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        s_nneVarIfaceModel10SetHeader_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc),
        s_nneVarIfaceModel10SetHeader_exit);

    //
    // Reset the NNE_VARS_VAR_ID_MAP.  Individual controllers will add
    // themselves as they construct.
    //
    nneVarsVarIdMapInit(NNE_VARS_VAR_ID_MAP_GET(pVars));

s_nneVarIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry()
 *
 * @memberof NNE_VARS
 *
 * @private
 */
static FLCN_STATUS
s_nneVarIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_FREQ:
            status = nneVarGrpIfaceModel10ObjSet_FREQ(pModel10, ppBoardObj,
                       sizeof(NNE_VAR_FREQ), pBuf);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_PM:
            status = nneVarGrpIfaceModel10ObjSet_PM(pModel10, ppBoardObj,
                       sizeof(NNE_VAR_PM), pBuf);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_CHIP_CONFIG:
            status = nneVarGrpIfaceModel10ObjSet_CHIP_CONFIG(pModel10, ppBoardObj,
                       sizeof(NNE_VAR_CHIP_CONFIG), pBuf);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_DN:
            status = nneVarGrpIfaceModel10ObjSet_POWER_DN(pModel10, ppBoardObj,
                       sizeof(NNE_VAR_POWER_DN), pBuf);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_TOTAL:
            status = nneVarGrpIfaceModel10ObjSet_POWER_TOTAL(pModel10, ppBoardObj,
                       sizeof(NNE_VAR_POWER_TOTAL), pBuf);
            break;
        default:
            PMU_TRUE_BP();
            status = FLCN_ERR_NOT_SUPPORTED;
            goto s_nneVarIfaceModel10SetEntry_exit;
            break;
    }

s_nneVarIfaceModel10SetEntry_exit:
    return status;
}

/*!
 * @brief Constructor.
 *
 * @memberof NNE_VAR
 *
 * @protected
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
nneVarGrpIfaceModel10ObjSet_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc),
        nneVarGrpIfaceModel10ObjSet_SUPER_exit);

nneVarGrpIfaceModel10ObjSet_SUPER_exit:
    return status;
}

/*!
 * @copydoc NneVarInputIDMatch
 */
FLCN_STATUS
nneVarInputIDMatch // NJ?? does not seem to be used
(
    NNE_VAR                         *pVar,
    LW2080_CTRL_NNE_NNE_VAR_INPUT   *pInput,
    LwBool                          *pBMatch
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if ((pVar    == NULL) ||
        (pInput  == NULL) ||
        (pBMatch == NULL))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneVarInputIDMatch_exit;
    }

    // If the types don't match, return.
    if (BOARDOBJ_GET_TYPE(&(pVar->super)) != pInput->type)
    {
        *pBMatch = LW_FALSE;
        goto nneVarInputIDMatch_exit;
    }

    switch (BOARDOBJ_GET_TYPE(pVar))
    {
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_FREQ:
            status = nneVarInputIDMatch_FREQ(pVar, pInput, pBMatch);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_PM:
            status = nneVarInputIDMatch_PM(pVar, pInput, pBMatch);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_CHIP_CONFIG:
            status = nneVarInputIDMatch_CHIP_CONFIG(pVar, pInput, pBMatch);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_DN:
            status = nneVarInputIDMatch_POWER_DN(pVar, pInput, pBMatch);
            break;
        case LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_TOTAL:
            status = nneVarInputIDMatch_POWER_TOTAL(pVar, pInput, pBMatch);
            break;
        default:
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto nneVarInputIDMatch_exit;
            break;
    }

nneVarInputIDMatch_exit:
    return status;
}
