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
 * @file clk_enum.c
 *
 * @brief Module managing all state related to the CLK_ENUM structure.
 *
 * @ref - confluence.lwpu.com/display/RMPER/Clock+Enumeration
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
static BoardObjGrpIfaceModel10SetEntry  (s_clkEnumIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkEnumIfaceModel10SetEntry");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_ENUM handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkEnumBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status;

    status = BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E255,   // _grpType
        CLK_ENUM,                                   // _class
        pBuffer,                                    // _pBuffer
        boardObjGrpIfaceModel10SetHeader,                 // _hdrFunc
        s_clkEnumIfaceModel10SetEntry,                    // _entryFunc
        ga10xAndLater.clk.clkEnumGrpSet,            // _ssElement
        OVL_INDEX_DMEM(perf),                       // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkEnumBoardObjGrpIfaceModel10Set_exit;
    }

    // If we provide set control, please trigger clk ilwalidation chain.

clkEnumBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * CLK_ENUM class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkEnumGrpIfaceModel10ObjSet_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_CLK_CLK_ENUM_BOARDOBJ_SET *pEnumSet =
        (RM_PMU_CLK_CLK_ENUM_BOARDOBJ_SET *)pBoardObjSet;
    CLK_ENUM       *pEnum;
    FLCN_STATUS     status;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkEnumGrpIfaceModel10ObjSet_SUPER_exit;
    }
    pEnum = (CLK_ENUM *)*ppBoardObj;

    // Copy the CLK_ENUM-specific data.
    pEnum->bOCOVEnabled     = pEnumSet->bOCOVEnabled;
    pEnum->freqMinMHz       = pEnumSet->freqMinMHz;
    pEnum->freqMaxMHz       = pEnumSet->freqMaxMHz;
    pEnum->offsetFreqMinMHz = pEnumSet->freqMinMHz; // Init to default
    pEnum->offsetFreqMaxMHz = pEnumSet->freqMaxMHz; // Init to default

clkEnumGrpIfaceModel10ObjSet_SUPER_exit:
    return status;
}

/*!
 * @ref ClkEnumFreqsIterate
 */
FLCN_STATUS
clkEnumFreqsIterate
(
    CLK_ENUM               *pEnum,
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    LwU16                  *pFreqMHz
)
{
    FLCN_STATUS status = FLCN_OK;

    // For the first time, return the MAX value.
    if ((*pFreqMHz) == CLK_DOMAIN_PROG_ITERATE_MAX_FREQ)
    {
         (*pFreqMHz) = pEnum->offsetFreqMaxMHz;
        goto clkEnumFreqsIterate_exit;
    }

    //
    // If Frequency is less than or equal to the min allowed frequency,
    // bail early.
    //
    if ((*pFreqMHz) <= pEnum->offsetFreqMinMHz)
    {
        status = FLCN_ERR_ITERATION_END;
        goto clkEnumFreqsIterate_exit;
    }

    //
    // If the enumeration entry contains only one valid frequency
    // point, return it.
    //
    if (pEnum->offsetFreqMaxMHz == pEnum->offsetFreqMinMHz)
    {
        (*pFreqMHz) = pEnum->offsetFreqMaxMHz;
        goto clkEnumFreqsIterate_exit;
    }

    //
    // Otherwise, call to CLK code to allow them to quantize within
    // min/max range.
    //
    status = clkGetNextLowerSupportedFreq(
                clkDomainApiDomainGet(pDomain40Prog),
                pFreqMHz);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkEnumFreqsIterate_exit;
    }
    //
    // Explicitly not colwerting FLCN_ERR_NOT_SUPPORTED -> FLCN_OK
    // here as we are in @ref clkEnumFreqQuantize(), as this interface
    // should never be called on non-NAFLL clock domains.  Those
    // domains should always use single-frequency enumeration points
    // for now.  If we want to support those going forward, we will
    // need Clk3.x to implement proper quantization and enumeration
    // support.
    //

clkEnumFreqsIterate_exit:
    return status;
}

/*!
 * @copydoc ClkEnumFreqQuantize
 */
FLCN_STATUS
clkEnumFreqQuantize
(
    CLK_ENUM                   *pEnum,
    CLK_DOMAIN_40_PROG         *pDomain40Prog,
    LW2080_CTRL_CLK_VF_INPUT   *pInputFreq,
    LW2080_CTRL_CLK_VF_OUTPUT  *pOutputFreq,
    LwBool                      bFloor,
    LwBool                      bBound
)
{
    FLCN_STATUS status = FLCN_OK;

    // If frequency is already quantized, return.
    if (pInputFreq->value == pEnum->offsetFreqMaxMHz)
    {
        pOutputFreq->value          = pEnum->offsetFreqMaxMHz;
        pOutputFreq->inputBestMatch = pOutputFreq->value;
        goto clkEnumFreqQuantize_exit;
    }
    else if (pInputFreq->value == pEnum->offsetFreqMinMHz)
    {
        pOutputFreq->value          = pEnum->offsetFreqMinMHz;
        pOutputFreq->inputBestMatch = pOutputFreq->value;
        goto clkEnumFreqQuantize_exit;
    }

    // If reqested, bound to max frequency.
    if ((bBound) &&
        (pInputFreq->value > pEnum->offsetFreqMaxMHz))
    {
        // If floor (rounding down), quantize to freqMaxMHz and return.
        if (bFloor)
        {
            pOutputFreq->value = pEnum->offsetFreqMaxMHz;
            goto clkEnumFreqQuantize_exit;
        }
        //
        // If ceiling and frequency is greater than freqMaxMHz, the frequency is
        // outside of the range of and cannot be quantized by this CLK_ENUM
        // object. Should try with the next CLK_ENUM object.
        //
        else
        {
            // If requested, set default value.
            if (FLD_TEST_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS, _VF_POINT_SET_DEFAULT,
                    _YES, pInputFreq->flags))
            {
                pOutputFreq->value = pEnum->offsetFreqMaxMHz;
            }
            status = FLCN_ERR_OUT_OF_RANGE;
            goto clkEnumFreqQuantize_exit;
        }
    }

    // If reqested, bound to min frequency.
    if ((bBound) &&
        (pInputFreq->value < pEnum->offsetFreqMinMHz))
    {
        // If ceiling (rounding up), quantize to freqMinMHz and early return.
        if (!bFloor)
        {
            pOutputFreq->value          = pEnum->offsetFreqMinMHz;
            pOutputFreq->inputBestMatch = pOutputFreq->value;
        }
        //
        // If floor and frequency is less than freqMinMHz, the frequency is
        // outside of the range of and cannot be quantized by this CLK_ENUM
        // object. Should END iteration here as we are searching from
        // min clk enumeration entry to max clk enumeration entry.
        //
        else
        {
            //
            // Handle case where input freq > max Freq of last CLK_ENUM.
            // Set bestMatch to max frequency to stop searching.
            //
            if ((pOutputFreq->inputBestMatch == LW2080_CTRL_CLK_VF_VALUE_ILWALID) &&
                (pOutputFreq->value != LW2080_CTRL_CLK_VF_VALUE_ILWALID))
            {
                pOutputFreq->inputBestMatch = pOutputFreq->value;
            }
            // If requested and not already set, set default value.
            else if ((FLD_TEST_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS, _VF_POINT_SET_DEFAULT,
                    _YES, pInputFreq->flags)) &&
                (pOutputFreq->inputBestMatch == LW2080_CTRL_CLK_VF_VALUE_ILWALID))
            {
                pOutputFreq->value          = pEnum->offsetFreqMinMHz;
                pOutputFreq->inputBestMatch = pOutputFreq->value;
            }
        }
        status = FLCN_OK;
        goto clkEnumFreqQuantize_exit;
    }

    pOutputFreq->value = pInputFreq->value;

    status = clkQuantizeFreq(
                clkDomainApiDomainGet(pDomain40Prog),
                ((LwU16*)&(pOutputFreq->value)),
                bFloor);
    if (status != FLCN_OK)
    {
        if (status == FLCN_ERR_NOT_SUPPORTED)
        {
            //
            // This is a special case where OBJCLK code will return
            // _NOT_SUPPORTED for non NAFLL clock domains.
            //
            status = FLCN_OK;
        }
        else
        {
            PMU_BREAKPOINT();
            goto clkEnumFreqQuantize_exit;
        }
    }

    // Update the best match and end iteration.
    pOutputFreq->inputBestMatch = pOutputFreq->value;

clkEnumFreqQuantize_exit:
    return status;
}

/*!
 * @copydoc ClkEnumFreqRangeAdjust
 */
FLCN_STATUS
clkEnumFreqRangeAdjust
(
    CLK_ENUM               *pEnum,
    CLK_DOMAIN_40_PROG     *pDomain40Prog
)
{
    FLCN_STATUS status = FLCN_OK;

    // Reset the offseted frequency range.
    pEnum->offsetFreqMinMHz = pEnum->freqMinMHz;
    pEnum->offsetFreqMaxMHz = pEnum->freqMaxMHz;

    // Adjust frequency enumeration only if enabled via POR.
    if (clkEnumOVOCEnabled(pEnum))
    {
        status = clkDomainProgClientFreqDeltaAdj_40(
                    CLK_DOMAIN_PROG_GET_FROM_40_PROG(pDomain40Prog),    // pDomainProg
                    &pEnum->offsetFreqMinMHz);                          // pFreqMHz
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkEnumFreqRangeAdjust_exit;
        }

        status = clkDomainProgClientFreqDeltaAdj_40(
                    CLK_DOMAIN_PROG_GET_FROM_40_PROG(pDomain40Prog),    // pDomainProg
                    &pEnum->offsetFreqMaxMHz);                          // pFreqMHz
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkEnumFreqRangeAdjust_exit;
        }
    }

clkEnumFreqRangeAdjust_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * CLK_ENUM implementation to parse BOARDOBJ entry.
 *
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_clkEnumIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    return clkEnumGrpIfaceModel10ObjSet_SUPER(pModel10,
                                  ppBoardObj,
                                  sizeof(CLK_ENUM),
                                  pBoardObjSet);
}
