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
 * @file clk_vf_rel.c
 *
 * @brief Module managing all state related to the CLK_VF_REL structure. This
 * structure defines how to generate the necessary VF mapping, per the VBIOS
 * specification.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
static BoardObjGrpIfaceModel10SetHeader (s_clkVfRelIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkVfRelIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry  (s_clkVfRelIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkVfRelIfaceModel10SetEntry");
static BoardObjIfaceModel10GetStatus              (s_clkVfRelIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "s_clkVfRelIfaceModel10GetStatus");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_VF_REL handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkVfRelBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status;
    LwBool      bFirstTime = (Clk.vfRels.super.super.objSlots == 0U);

    // Increment the VF Points cache counter due to change in CLK_VF_RELs params
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT))
    {
        clkVfPointsVfPointsCacheCounterIncrement();
    }

    status = BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E255,   // _grpType
        CLK_VF_REL,                                 // _class
        pBuffer,                                    // _pBuffer
        s_clkVfRelIfaceModel10SetHeader,                  // _hdrFunc
        s_clkVfRelIfaceModel10SetEntry,                   // _entryFunc
        ga10xAndLater.clk.clkVfRelGrpSet,           // _ssElement
        OVL_INDEX_DMEM(perf),                       // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfRelBoardObjGrpIfaceModel10Set_exit;
    }

    // Ilwalidate the CLK VF_RELs on SET CONTROL.
    if (!bFirstTime)
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfIlwalidation)
        };

        // VFE evaluation is not required.
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = clkIlwalidate(LW_FALSE);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfRelBoardObjGrpIfaceModel10Set_exit;
        }
    }

clkVfRelBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * CLK_VF_REL handler for the RM_PMU_BOARDOBJ_CMD_GRP GET_STATUS interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkVfRelBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS_AUTO_DMA(E255,    // _grpType
        CLK_VF_REL,                                 // _class
        pBuffer,                                    // _bu
        NULL,                                       // _hf
        s_clkVfRelIfaceModel10GetStatus,                        // _ef
        ga10xAndLater.clk.clkVfRelGrpGetStatus);    // _sse
}

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkVfRelGrpIfaceModel10ObjSet_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_CLK_CLK_VF_REL_BOARDOBJ_SET *pVfRelSet =
        (RM_PMU_CLK_CLK_VF_REL_BOARDOBJ_SET *)pBoardObjSet;
    CLK_VF_REL *pVfRel;
    FLCN_STATUS status;

    //
    // Call into BOARDOBJ super-constructor, which will do actual memory
    // allocation.
    //
    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        // Super class triggers PMU Break point on error.
        goto clkVfRelGrpIfaceModel10ObjSet_SUPER_exit;
    }
    pVfRel = (CLK_VF_REL *)*ppBoardObj;

    // Copy the CLK_VF_REL-specific data.
    pVfRel->railIdx             = pVfRelSet->railIdx;
    pVfRel->bOCOVEnabled        = pVfRelSet->bOCOVEnabled;
    pVfRel->freqMaxMHz          = pVfRelSet->freqMaxMHz;
    pVfRel->offsettedFreqMaxMHz = pVfRelSet->freqMaxMHz; // Init to POR
    pVfRel->voltDeltauV         = pVfRelSet->voltDeltauV;
    pVfRel->freqDelta           = pVfRelSet->freqDelta;
    pVfRel->vfEntryPri          = pVfRelSet->vfEntryPri;

    memcpy(pVfRel->vfEntriesSec, pVfRelSet->vfEntriesSec,
        (sizeof(LW2080_CTRL_CLK_CLK_VF_REL_VF_ENTRY_SEC)
            * LW2080_CTRL_CLK_CLK_VF_REL_VF_ENTRY_SEC_MAX));

clkVfRelGrpIfaceModel10ObjSet_SUPER_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelLoad
 */
FLCN_STATUS
clkVfRelLoad
(
    CLK_VF_REL         *pVfRel,
    CLK_DOMAIN_40_PROG *pDomain40Prog,
    LwU8                lwrveIdx
)
{
    CLK_VF_POINT_40    *pVfPoint40;
    FLCN_STATUS         status = FLCN_OK;
    LwBoardObjIdx       vfIdx;

    // Sanity check.
    if ((LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_ILWALID == lwrveIdx) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx)) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkVfRelLoad_exit;
    }

    //
    // Iterate over this CLK_VF_REL's CLK_VF_POINTs and load each of
    // them in order.
    //
    for (vfIdx = clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx);
         vfIdx <= clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx);
         vfIdx++)
    {
        pVfPoint40 = (CLK_VF_POINT_40 *)CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx, vfIdx);
        if (pVfPoint40 == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkVfRelLoad_exit;
        }

        status = clkVfPoint40Load(pVfPoint40,
                                  pDomain40Prog,
                                  pVfRel,
                                  lwrveIdx);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfRelLoad_exit;
        }
    }

clkVfRelLoad_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelVoltToFreq
 */
FLCN_STATUS
clkVfRelVoltToFreq
(
    CLK_VF_REL                 *pVfRel,
    CLK_DOMAIN_40_PROG         *pDomain40Prog,
    LwU8                        voltageType,
    LW2080_CTRL_CLK_VF_INPUT   *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT  *pOutput
)
{
    CLK_VF_POINT_40         *pVfPoint40;
    LW2080_CTRL_CLK_VF_PAIR  vfPair  = LW2080_CTRL_CLK_VF_PAIR_INIT();
    LwBool          bValidMatchFound     = LW_FALSE;
    LwU8            lwrveIdx = LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI;
    LwBoardObjIdx   midIdx   = LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID;
    LwBoardObjIdx   startIdx = clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx);
    LwBoardObjIdx   endIdx   = clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx);
    FLCN_STATUS     status   = FLCN_OK;

    // Only VF_REL_TYPE_RATIO_VOLT support _SOURCE voltage type.
    if ((LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE == voltageType) &&
        (LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_VOLT !=
            BOARDOBJ_GET_TYPE(pVfRel)))
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto clkVfRelVoltToFreq_exit;
    }

    //
    // Binary search through set of CLK_VF_POINTs for this CLK_VF_REL
    // to find best match.
    //
    do
    {
        //
        // We want to round up to the higher index if (startIdx + endIdx)
        // is not exactly divisible by 2. This is to ensure that we always
        // test the highest index.
        //
        midIdx   = LW_UNSIGNED_ROUNDED_DIV((startIdx + endIdx), 2U);

        pVfPoint40 = (CLK_VF_POINT_40 *)CLK_VF_POINT_GET(PRI, midIdx);
        if (pVfPoint40 == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkVfRelVoltToFreq_exit;
        }

        //
        // Retrieve the voltage and frequency for this CLK_VF_POINT via the
        // helper accessor.
        //
        status = clkVfPoint40Accessor(pVfPoint40,       // pVfPoint40
                                      pDomain40Prog,    // pDomain40Prog
                                      pVfRel,           // pVfRel
                                      lwrveIdx,         // lwrveIdx
                                      voltageType,      // voltageType
                                      LW_TRUE,          // bOffseted
                                      &vfPair);         // pVfPair
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfRelVoltToFreq_exit;
        }

        // If the exact crossover point found, break out.
        if ((vfPair.voltageuV == pInput->value) ||
            (endIdx == startIdx))
        {
            LW2080_CTRL_CLK_VF_PAIR vfPairNext = LW2080_CTRL_CLK_VF_PAIR_INIT();

            if (midIdx == clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx))
            {
                //
                // If the exact match found, update the flag. Do not terminate
                // the algorithm here as we observed that multiple clock vf rels
                // could have exact same voltage requirements. Also if the input
                // voltage is higher then the voltage of last VF point consider
                // last VF point as valid match. Must terminate the algorithm only
                // when the cross over point is found.
                //
                if (vfPair.voltageuV <= pInput->value)
                {
                    bValidMatchFound = LW_TRUE;
                }

                // Always break as this is end of search.
                break;
            }
            else if ((midIdx == clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx)) &&
                     (vfPair.voltageuV > pInput->value))
            {
                // Input voltage is outside of the VF lwrve, terminate the algorithm.
                status = FLCN_ERR_ITERATION_END;
                break;
            }

            //
            // Check next VF point to ensure we found the crossover point
            // as we want Fmax @ V.
            //
            pVfPoint40 = (CLK_VF_POINT_40 *)CLK_VF_POINT_GET(PRI, (midIdx + 1U));
            if (pVfPoint40 == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto clkVfRelVoltToFreq_exit;
            }

            //
            // Retrieve the voltage and frequency for this CLK_VF_POINT via the
            // helper accessor.
            //
            status = clkVfPoint40Accessor(pVfPoint40,       // pVfPoint40
                                          pDomain40Prog,    // pDomain40Prog
                                          pVfRel,           // pVfRel
                                          lwrveIdx,         // lwrveIdx
                                          voltageType,      // voltageType
                                          LW_TRUE,          // bOffseted
                                          &vfPairNext);     // pVfPair
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkVfRelVoltToFreq_exit;
            }

            if (vfPairNext.voltageuV > pInput->value)
            {
                //
                // If we reached end of search with two closest points,
                // this algorithm will select the lower VF point to close
                // the while loop. As we are looking for Fmax @ V, this
                // is the correct VF point.
                //
                bValidMatchFound = LW_TRUE;
                status = FLCN_ERR_ITERATION_END;
                break;
            }

            // Break if we reached the end of search.
            if (endIdx == startIdx)
            {
                break;
            }
        }

        //
        // If not found, update the indexes.
        // In case of V -> F we are looking for Fmax @ V, so exact
        // match will be consider below if it is not also a crossover
        // VF point.
        //
        if (vfPair.voltageuV <= pInput->value)
        {
            startIdx = midIdx;
        }
        else
        {
            endIdx = (midIdx - 1U);
        }
    } while (LW_TRUE);

    if (bValidMatchFound)
    {
        pOutput->inputBestMatch = vfPair.voltageuV;
        pOutput->value          = vfPair.freqMHz;
    }
    //
    // If the default flag is set, always update it.
    // If there is a better match it will be overwritten in the
    // next block.
    //
    else if ((FLD_TEST_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS, _VF_POINT_SET_DEFAULT, _YES,
                pInput->flags)) &&
        (pOutput->inputBestMatch == LW2080_CTRL_CLK_VF_VALUE_ILWALID) &&
        (pOutput->value == LW2080_CTRL_CLK_VF_VALUE_ILWALID))
    {
        pOutput->inputBestMatch = vfPair.voltageuV;
        pOutput->value          = vfPair.freqMHz;
    }

clkVfRelVoltToFreq_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelFreqToVolt
 */
FLCN_STATUS
clkVfRelFreqToVolt
(
    CLK_VF_REL                 *pVfRel,
    CLK_DOMAIN_40_PROG         *pDomain40Prog,
    LwU8                        voltageType,
    LW2080_CTRL_CLK_VF_INPUT   *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT  *pOutput
)
{
    CLK_VF_POINT_40         *pVfPoint40;
    LW2080_CTRL_CLK_VF_PAIR  vfPair  = LW2080_CTRL_CLK_VF_PAIR_INIT();
    LwBool          bValidMatchFound     = LW_FALSE;
    LwU8            lwrveIdx = LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI;
    LwBoardObjIdx   midIdx   = LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID;
    LwBoardObjIdx   startIdx = clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx);
    LwBoardObjIdx   endIdx   = clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx);
    FLCN_STATUS     status   = FLCN_OK;

    // Only VF_REL_TYPE_RATIO_VOLT support _SOURCE voltage type.
    if ((LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE == voltageType) &&
        (LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_VOLT !=
            BOARDOBJ_GET_TYPE(pVfRel)))
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto clkVfRelFreqToVolt_exit;
    }

    //
    // Binary search through set of CLK_VF_POINTs for this CLK_VF_REL
    // to find best match.
    //
    do
    {
        //
        // We want to round up to the higher index if (startIdx + endIdx)
        // is not exactly divisible by 2. This is to ensure that we always
        // test the highest index.
        //
        midIdx   = LW_UNSIGNED_ROUNDED_DIV((startIdx + endIdx), 2U);

        pVfPoint40 = (CLK_VF_POINT_40 *)CLK_VF_POINT_GET(PRI, midIdx);
        if (pVfPoint40 == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkVfRelFreqToVolt_exit;
        }

        //
        // Retrieve the voltage and frequency for this CLK_VF_POINT via the
        // helper accessor.
        //
        status = clkVfPoint40Accessor(pVfPoint40,       // pVfPoint40
                                      pDomain40Prog,    // pDomain40Prog
                                      pVfRel,           // pVfRel
                                      lwrveIdx,         // lwrveIdx
                                      voltageType,      // voltageType
                                      LW_TRUE,          // bOffseted
                                      &vfPair);         // pVfPair
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfRelFreqToVolt_exit;
        }

        // If the exact crossover point found, break out.
        if ((vfPair.freqMHz == pInput->value) ||
            (endIdx == startIdx))
        {
            LW2080_CTRL_CLK_VF_PAIR vfPairPrev = LW2080_CTRL_CLK_VF_PAIR_INIT();

            if (midIdx == clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx))
            {
                //
                // If the input frequency is below the frequency of first VF point,
                // consider first VF point as valid match.
                //
                if (vfPair.freqMHz >= pInput->value)
                {
                    bValidMatchFound = LW_TRUE;

                    // Terminate the algorithm here as we are looking for Vmin.
                    status = FLCN_ERR_ITERATION_END;
                }
                else if (midIdx < clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx))
                {
                    pVfPoint40 = (CLK_VF_POINT_40 *)CLK_VF_POINT_GET(PRI, (midIdx + 1U));
                    if (pVfPoint40 == NULL)
                    {
                        PMU_BREAKPOINT();
                        status = FLCN_ERR_ILWALID_INDEX;
                        goto clkVfRelFreqToVolt_exit;
                    }

                    status = clkVfPoint40Accessor(pVfPoint40,       // pVfPoint40
                                                  pDomain40Prog,    // pDomain40Prog
                                                  pVfRel,           // pVfRel
                                                  lwrveIdx,         // lwrveIdx
                                                  voltageType,      // voltageType
                                                  LW_TRUE,          // bOffseted
                                                  &vfPair);         // pVfPair
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                        goto clkVfRelFreqToVolt_exit;
                    }

                    // Algorithm is expected to colwerge on two closest matching point.
                    if (vfPair.freqMHz < pInput->value)
                    {
                        status = FLCN_ERR_ILWALID_STATE;
                        PMU_BREAKPOINT();
                        goto clkVfRelFreqToVolt_exit;
                    }
                    bValidMatchFound = LW_TRUE;

                    // Terminate the algorithm here as we are looking for Vmin.
                    status = FLCN_ERR_ITERATION_END;
                }
                // Always break as we reached the end of search.
                break;
            }
            else if ((midIdx == clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx)) &&
                     (vfPair.freqMHz < pInput->value))
            {
                // Input frequency is outside of the VF lwrve check next clk vf rel.
                break;
            }

            //
            // In case of crossover, the midIdx MUST point to higher
            // frequency then input as we are doing round up in our
            // search algorithm. So if prev F point points to lower
            // frequency, we can consider this VF point as valid match
            // for Vmin @ F
            //
            pVfPoint40 = (CLK_VF_POINT_40 *)CLK_VF_POINT_GET(PRI, (midIdx - 1U));
            if (pVfPoint40 == NULL)
            {
                status = FLCN_ERR_ILWALID_INDEX;
                PMU_BREAKPOINT();
                goto clkVfRelFreqToVolt_exit;
            }

            //
            // Retrieve the voltage and frequency for this CLK_VF_POINT via the
            // helper accessor.
            //
            status = clkVfPoint40Accessor(pVfPoint40,       // pVfPoint40
                                          pDomain40Prog,    // pDomain40Prog
                                          pVfRel,           // pVfRel
                                          lwrveIdx,         // lwrveIdx
                                          voltageType,      // voltageType
                                          LW_TRUE,          // bOffseted
                                          &vfPairPrev);     // pVfPair
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkVfRelFreqToVolt_exit;
            }

            if (vfPairPrev.freqMHz < pInput->value)
            {
                //
                // If we reached end of search with two closest points,
                // this algorithm will select the lower VF point to close
                // the while loop. As we are looking for Vmin @ F, we
                // must pick the higher VF point in this case.
                //
                if (vfPair.freqMHz < pInput->value)
                {
                    pVfPoint40 = (CLK_VF_POINT_40 *)CLK_VF_POINT_GET(PRI, (midIdx + 1U));
                    if (pVfPoint40 == NULL)
                    {
                        status = FLCN_ERR_ILWALID_INDEX;
                        PMU_BREAKPOINT();
                        goto clkVfRelFreqToVolt_exit;
                    }

                    status = clkVfPoint40Accessor(pVfPoint40,       // pVfPoint40
                                                  pDomain40Prog,    // pDomain40Prog
                                                  pVfRel,           // pVfRel
                                                  lwrveIdx,         // lwrveIdx
                                                  voltageType,      // voltageType
                                                  LW_TRUE,          // bOffseted
                                                  &vfPair);         // pVfPair
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                        goto clkVfRelFreqToVolt_exit;
                    }
                }

                bValidMatchFound = LW_TRUE;
                status = FLCN_ERR_ITERATION_END;
                break;
            }

            // Break if we reached the end of search.
            if (endIdx == startIdx)
            {
                break;
            }
        }

        //
        // If not found, update the indexes.
        // In case of F -> V we are looking for Vmin @ F, so exact
        // match will be consider above if it is not also a crossover
        // VF point.
        //
        if (vfPair.freqMHz < pInput->value)
        {
            startIdx = midIdx;
        }
        else
        {
            endIdx = (midIdx - 1U);
        }
    } while (LW_TRUE);

    // If the exact match found, short-circuit the search.
    if (bValidMatchFound)
    {
        pOutput->inputBestMatch = vfPair.freqMHz;
        pOutput->value          = vfPair.voltageuV;
    }
    //
    // If the default flag is set, always update it.
    // If there is a better match it will be overwritten in the
    // next block.
    //
    else if ((FLD_TEST_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS, _VF_POINT_SET_DEFAULT, _YES,
                pInput->flags)) &&
            LW2080_CTRL_CLK_VF_VALUE_ILWALID == pOutput->inputBestMatch &&
            LW2080_CTRL_CLK_VF_VALUE_ILWALID == pOutput->value &&
            (midIdx  == clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx)))
    {
        //
        // For Freq -> Volt default value is the last VF point of the
        // last clock domain.
        //
        pOutput->inputBestMatch = vfPair.freqMHz;
        pOutput->value          = vfPair.voltageuV;
    }

clkVfRelFreqToVolt_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelVoltToFreqTupleVfIdxGet
 */
FLCN_STATUS
clkVfRelVoltToFreqTupleVfIdxGet
(
    CLK_VF_REL                                     *pVfRel,
    CLK_DOMAIN_40_PROG                             *pDomain40Prog,
    LwU8                                            voltageType,
    LW2080_CTRL_CLK_VF_INPUT                       *pInput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE             *pVfIterState
)
{
    CLK_VF_POINT_40         *pVfPoint40;
    LW2080_CTRL_CLK_VF_PAIR *pVfPair  = &pVfIterState->vfPair;
    LwBoardObjIdx            vfIdx;
    LwU8                     lwrveIdx = LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI;
    FLCN_STATUS              status   = FLCN_OK;

    // Only VF_REL_TYPE_RATIO_VOLT support _SOURCE voltage type.
    if ((LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE == voltageType) &&
        (LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_VOLT !=
            BOARDOBJ_GET_TYPE(pVfRel)))
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        PMU_BREAKPOINT();
        goto clkVfRelVoltToFreqTupleVfIdxGet_exit;
    }

    // Iterate over this CLK_VF_REL's CLK_VF_POINTs to find the best match.
    for (vfIdx = LW_MAX(clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx),
                    pVfIterState->clkVfPointIdx);
         vfIdx <= clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx);
         vfIdx++)
    {
        pVfPoint40 = (CLK_VF_POINT_40 *)CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx, vfIdx);
        if (pVfPoint40 == NULL)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            PMU_BREAKPOINT();
            goto clkVfRelVoltToFreqTupleVfIdxGet_exit;
        }

        status = clkVfPoint40Accessor(pVfPoint40,       // pVfPoint40
                                      pDomain40Prog,    // pDomain40Prog
                                      pVfRel,           // pVfRel
                                      lwrveIdx,         // lwrveIdx
                                      voltageType,      // voltageType
                                      LW_TRUE,          // bOffseted
                                      pVfPair);         // pVfPair
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfRelVoltToFreqTupleVfIdxGet_exit;
        }

        //
        // Once VF points above the input value, search is done. Short-circuit the
        // search here.
        //
        if (pVfPair->voltageuV > pInput->value)
        {
            //
            // If the first VF point of this VF Rel is above the input
            // voltage than we must update our VF iterator with the
            // last VF point of previous VF Rel. If this is the first
            // VF Rel index of this clock domian than it is Error unless
            // user explicitly requested default values.
            //
            if (vfIdx == clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx))
            {
                status = FLCN_ERR_OUT_OF_RANGE;
            }
            else
            {
                status = FLCN_ERR_ITERATION_END;
            }

            // Always break.
            break;
        }

        // Save off the last matching VF_POINT index to iteration state.
        pVfIterState->clkVfPointIdx = vfIdx;
    }

clkVfRelVoltToFreqTupleVfIdxGet_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelVoltToFreqTuple
 */
FLCN_STATUS
clkVfRelVoltToFreqTuple
(
    CLK_VF_REL                                     *pVfRel,
    CLK_DOMAIN_40_PROG                             *pDomain40Prog,
    LwBoardObjIdx                                   clkVfPointIdx,
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE    *pOutput
)
{
    CLK_VF_POINT_40         *pVfPoint40;
    LwU8                     lwrveIdx;
    FLCN_STATUS              status   = FLCN_OK;

    // Copy-in frequency tuple of all VF lwrves.
    for (lwrveIdx = LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI;
         lwrveIdx < clkDomain40ProgGetClkDomailwfLwrveCount(pDomain40Prog);
         lwrveIdx++)
    {
        // Adjust the position of VF point index from first VF point index.
        if (lwrveIdx != LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI)
        {
            clkVfPointIdx += clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx);
        }

        pVfPoint40 = (CLK_VF_POINT_40 *)CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx,
                                                                     clkVfPointIdx);
        if (pVfPoint40 == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkVfRelVoltToFreqTuple_exit;
        }

        // Get the frequency tuple from VF point index.
        status = clkVfPoint40FreqTupleAccessor(
                    pVfPoint40,                             // pVfPoint40
                    pDomain40Prog,                          // pDomain40Prog
                    pVfRel,                                 // pVfRel
                    lwrveIdx,                               // lwrveIdx
                    LW_TRUE,                                // bOffseted
                    pOutput);                               // pOutput
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfRelVoltToFreqTuple_exit;
        }
    }

clkVfRelVoltToFreqTuple_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelCache
 */
FLCN_STATUS
clkVfRelCache
(
    CLK_VF_REL         *pVfRel,
    CLK_DOMAIN_40_PROG *pDomain40Prog,
    LwBool              bVFEEvalRequired,
    LwU8                lwrveIdx,
    CLK_VF_POINT_40    *pVfPoint40Last
)
{
    CLK_VF_POINT_40        *pVfPoint40;
    FLCN_STATUS             status = FLCN_OK;
    LwBoardObjIdx           vfIdx;
    LW2080_CTRL_CLK_VF_PAIR vfPair;

    // Sanity check.
    if ((LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_ILWALID == lwrveIdx) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx)) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkVfRelCache_exit;
    }

    //
    // Iterate over this CLK_VF_REL's CLK_VF_POINTs and load each of
    // them in order.
    //
    for (vfIdx = clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx);
         vfIdx <= clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx);
         vfIdx++)
    {
        pVfPoint40 = (CLK_VF_POINT_40 *)CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx, vfIdx);
        if (pVfPoint40 == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkVfRelCache_exit;
        }

        status = clkVfPoint40Cache(pVfPoint40,
                                   pVfPoint40Last,
                                   pDomain40Prog,
                                   pVfRel,
                                   lwrveIdx,
                                   bVFEEvalRequired);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfRelCache_exit;
        }

        // Update the last VF point pointer.
        pVfPoint40Last = pVfPoint40;
    }

    // Update the VF REL offset adjusted max based on last VF point.
    pVfPoint40 = (CLK_VF_POINT_40 *)CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx,
                                    clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx));
    if (pVfPoint40 == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkVfRelCache_exit;
    }

    // Get the last VF point frequency tuple.
    status = clkVfPoint40Accessor(pVfPoint40,                       // pVfPoint40
                                pDomain40Prog,                      // pDomain40Prog
                                pVfRel,                             // pVfRel
                                lwrveIdx,                           // lwrveIdx
                                LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,   // voltageType
                                LW_TRUE,                            // bOffseted
                                &vfPair);                           // pVfPair
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfRelCache_exit;
    }

    //
    // Set the offset adjusted frequency equal to the last VF point offset
    // adjusted frequency. This is acceptable as VF lwrve are trimmed to
    // MIN (VF_REL::freqMaxMHz, ENUM::freqMaxMHz)
    //
    pVfRel->offsettedFreqMaxMHz = vfPair.freqMHz;

clkVfRelCache_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelOffsetVFCache
 */
FLCN_STATUS
clkVfRelOffsetVFCache
(
    CLK_VF_REL         *pVfRel,
    CLK_DOMAIN_40_PROG *pDomain40Prog,
    LwU8                lwrveIdx,
    CLK_VF_POINT_40    *pVfPoint40Last
)
{
    CLK_VF_POINT_40        *pVfPoint40;
    FLCN_STATUS             status = FLCN_OK;
    LwBoardObjIdx           vfIdx;

    // Sanity check.
    if ((LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_ILWALID == lwrveIdx) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx)) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkVfRelOffsetVFCache_exit;
    }

    //
    // Iterate over this CLK_VF_REL's CLK_VF_POINTs and load each of
    // them in order.
    //
    for (vfIdx = clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx);
         vfIdx <= clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx);
         vfIdx++)
    {
        pVfPoint40 = (CLK_VF_POINT_40 *)CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx, vfIdx);
        if (pVfPoint40 == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkVfRelOffsetVFCache_exit;
        }

        status = clkVfPoint40OffsetVFCache(pVfPoint40,
                                           pVfPoint40Last,
                                           pDomain40Prog,
                                           pVfRel);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfRelOffsetVFCache_exit;
        }

        // Update the last VF point pointer.
        // Used for monotonoicity
        // TODO: remove if we do not keep the offset lwrve monotonic
        pVfPoint40Last = pVfPoint40;
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfRelOffsetVFCache_exit;
    }

clkVfRelOffsetVFCache_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelBaseVFCache
 */
FLCN_STATUS
clkVfRelBaseVFCache
(
    CLK_VF_REL         *pVfRel,
    CLK_DOMAIN_40_PROG *pDomain40Prog,
    LwU8                lwrveIdx,
    LwU8                cacheIdx,
    CLK_VF_POINT_40    *pVfPoint40Last
)
{
    CLK_VF_POINT_40        *pVfPoint40;
    FLCN_STATUS             status = FLCN_OK;
    LwBoardObjIdx           vfIdx;

    // Sanity check.
    if ((LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_ILWALID == lwrveIdx) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx)) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkVfRelBaseVFCache_exit;
    }

    //
    // Iterate over this CLK_VF_REL's CLK_VF_POINTs and load each of
    // them in order.
    //
    for (vfIdx = clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx);
         vfIdx <= clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx);
         vfIdx++)
    {
        pVfPoint40 = (CLK_VF_POINT_40 *)CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx, vfIdx);
        if (pVfPoint40 == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkVfRelBaseVFCache_exit;
        }

        status = clkVfPoint40BaseVFCache(pVfPoint40,
                                   pVfPoint40Last,
                                   pDomain40Prog,
                                   pVfRel,
                                   lwrveIdx,
                                   cacheIdx);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfRelBaseVFCache_exit;
        }

        // Update the last VF point pointer.
        pVfPoint40Last = pVfPoint40;
    }

clkVfRelBaseVFCache_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelSmoothing
 */
FLCN_STATUS
clkVfRelSmoothing
(
    CLK_VF_REL             *pVfRel,
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    LwU8                    lwrveIdx,
    LwU8                    cacheIdx,
    CLK_VF_POINT_40_VOLT   *pVfPoint40VoltLast
)
{

    switch (BOARDOBJ_GET_TYPE(pVfRel))
    {
        case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_VOLT:
        {
            return clkVfRelSmoothing_RATIO_VOLT(pVfRel,
                                                pDomain40Prog,
                                                lwrveIdx,
                                                cacheIdx,
                                                pVfPoint40VoltLast);
        }
    }

    return FLCN_OK;
}

/*!
 * @copydoc ClkVfRelSecondaryCache
 */
FLCN_STATUS
clkVfRelSecondaryCache
(
    CLK_VF_REL         *pVfRel,
    CLK_DOMAIN_40_PROG *pDomain40Prog,
    LwBool              bVFEEvalRequired,
    LwU8                lwrveIdx,
    CLK_VF_POINT_40    *pVfPoint40Last
)
{
    CLK_VF_POINT_40    *pVfPoint40;
    FLCN_STATUS         status = FLCN_OK;
    LwBoardObjIdx       vfIdx;

    // Sanity check.
    if ((LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_ILWALID == lwrveIdx) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx)) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkVfRelSecondaryCache_exit;
    }

    //
    // Iterate over this CLK_VF_REL's CLK_VF_POINTs and load each of
    // them in order.
    //
    for (vfIdx = clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx);
         vfIdx <= clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx);
         vfIdx++)
    {
        pVfPoint40 = (CLK_VF_POINT_40 *)CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx, vfIdx);
        if (pVfPoint40 == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkVfRelSecondaryCache_exit;
        }

        status = clkVfPoint40SecondaryCache(pVfPoint40,
                                        pVfPoint40Last,
                                        pDomain40Prog,
                                        pVfRel,
                                        lwrveIdx,
                                        bVFEEvalRequired);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfRelSecondaryCache_exit;
        }

        // Update the last VF point pointer.
        pVfPoint40Last = pVfPoint40;
    }

clkVfRelSecondaryCache_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelOffsetVFSecondaryCache
 */
FLCN_STATUS
clkVfRelSecondaryOffsetVFCache
(
    CLK_VF_REL         *pVfRel,
    CLK_DOMAIN_40_PROG *pDomain40Prog,
    LwU8                lwrveIdx,
    CLK_VF_POINT_40    *pVfPoint40Last
)
{
    CLK_VF_POINT_40    *pVfPoint40;
    FLCN_STATUS         status = FLCN_OK;
    LwBoardObjIdx       vfIdx;

    // Sanity check.
    if ((LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_ILWALID == lwrveIdx) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx)) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkVfRelSecondaryOffsetVFCache_exit;
    }

    //
    // Iterate over this CLK_VF_REL's CLK_VF_POINTs and load each of
    // them in order.
    //
    for (vfIdx = clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx);
         vfIdx <= clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx);
         vfIdx++)
    {
        pVfPoint40 = (CLK_VF_POINT_40 *)CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx, vfIdx);
        if (pVfPoint40 == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkVfRelSecondaryOffsetVFCache_exit;
        }

        status = clkVfPoint40SecondaryOffsetVFCache(pVfPoint40,
                                                pVfPoint40Last,
                                                pDomain40Prog,
                                                pVfRel,
                                                lwrveIdx);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfRelSecondaryOffsetVFCache_exit;
        }

        // Update the last VF point pointer.
        pVfPoint40Last = pVfPoint40;
    }

clkVfRelSecondaryOffsetVFCache_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelSecondaryBaseVFCache
 */
FLCN_STATUS
clkVfRelSecondaryBaseVFCache
(
    CLK_VF_REL         *pVfRel,
    CLK_DOMAIN_40_PROG *pDomain40Prog,
    LwU8                lwrveIdx,
    LwU8                cacheIdx,
    CLK_VF_POINT_40    *pVfPoint40Last
)
{
    CLK_VF_POINT_40    *pVfPoint40;
    FLCN_STATUS         status = FLCN_OK;
    LwBoardObjIdx       vfIdx;

    // Sanity check.
    if ((LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_ILWALID == lwrveIdx) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx)) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkVfRelSecondaryBaseVFCache_exit;
    }

    //
    // Iterate over this CLK_VF_REL's CLK_VF_POINTs and load each of
    // them in order.
    //
    for (vfIdx = clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx);
         vfIdx <= clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx);
         vfIdx++)
    {
        pVfPoint40 = (CLK_VF_POINT_40 *)CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx, vfIdx);
        if (pVfPoint40 == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkVfRelSecondaryBaseVFCache_exit;
        }

        status = clkVfPoint40SecondaryBaseVFCache(pVfPoint40,
                                        pVfPoint40Last,
                                        pDomain40Prog,
                                        pVfRel,
                                        lwrveIdx,
                                        cacheIdx);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfRelSecondaryBaseVFCache_exit;
        }

        // Update the last VF point pointer.
        pVfPoint40Last = pVfPoint40;
    }

clkVfRelSecondaryBaseVFCache_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelFreqTranslatePrimaryToSecondary
 */
FLCN_STATUS
clkVfRelFreqTranslatePrimaryToSecondary
(
    CLK_VF_REL     *pVfRel,
    LwU8            secondaryIdx,
    LwU16          *pFreqMHz,
    LwBool          bQuantize
)
{
    switch (BOARDOBJ_GET_TYPE(pVfRel))
    {
        case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_VOLT:
        case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_FREQ:
        {
            return clkVfRelFreqTranslatePrimaryToSecondary_RATIO(pVfRel,
                                                            secondaryIdx,
                                                            pFreqMHz,
                                                            bQuantize);
        }
        case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_TABLE_FREQ:
        {
            return clkVfRelFreqTranslatePrimaryToSecondary_TABLE(pVfRel,
                                                            secondaryIdx,
                                                            pFreqMHz,
                                                            bQuantize);
        }
    }

    // No type-specific implementation found.  Error!
    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc ClkVfRelFreqTranslatePrimaryToSecondary
 */
FLCN_STATUS
clkVfRelOffsetVFFreqTranslatePrimaryToSecondary
(
    CLK_VF_REL     *pVfRel,
    LwU8            secondaryIdx,
    LwS16          *pFreqMHz,
    LwBool          bQuantize
)
{
    switch (BOARDOBJ_GET_TYPE(pVfRel))
    {
        case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_VOLT:
        case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_FREQ:
        {
            return clkVfRelOffsetVFFreqTranslatePrimaryToSecondary_RATIO(pVfRel,
                                                                    secondaryIdx,
                                                                    pFreqMHz,
                                                                    bQuantize);
        }
        case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_TABLE_FREQ:
        {
            return clkVfRelOffsetVFFreqTranslatePrimaryToSecondary_TABLE(pVfRel,
                                                                    secondaryIdx,
                                                                    pFreqMHz,
                                                                    bQuantize);
        }
    }

    // No type-specific implementation found.  Error!
    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}
/*!
 * @copydoc ClkVfRelFreqTranslateSecondaryToPrimary
 */
FLCN_STATUS
clkVfRelFreqTranslateSecondaryToPrimary
(
    CLK_VF_REL     *pVfRel,
    LwU8            secondaryIdx,
    LwU16          *pFreqMHz
)
{
    switch (BOARDOBJ_GET_TYPE(pVfRel))
    {
        case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_VOLT:
        case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_FREQ:
        {
            return clkVfRelFreqTranslateSecondaryToPrimary_RATIO(pVfRel,
                                                            secondaryIdx,
                                                            pFreqMHz);
        }
        case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_TABLE_FREQ:
        {
            return clkVfRelFreqTranslateSecondaryToPrimary_TABLE(pVfRel,
                                                            secondaryIdx,
                                                            pFreqMHz);
        }
    }

    // No type-specific implementation found.  Error!
    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc ClkVfRelGetIdxFromFreq
 */
LwBool
clkVfRelGetIdxFromFreq
(
    CLK_VF_REL         *pVfRel,
    CLK_DOMAIN_40_PROG *pDomain40Prog,
    LwU16               freqMHz
)
{
    LwBool bValidMatch = LW_FALSE;

    if (clkDomain40ProgGetRailVfType(pDomain40Prog, pVfRel->railIdx) ==
        LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_PRIMARY)
    {
        if (freqMHz <= pVfRel->freqMaxMHz)
        {
            bValidMatch = LW_TRUE;
        }
    }
    else
    {
        switch (BOARDOBJ_GET_TYPE(pVfRel))
        {
            case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_VOLT:
            case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_FREQ:
            {
                bValidMatch = clkVfRelGetIdxFromFreq_RATIO(pVfRel,
                                                        pDomain40Prog,
                                                        freqMHz);
                break;
            }
            case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_TABLE_FREQ:
            {
                bValidMatch = clkVfRelGetIdxFromFreq_TABLE(pVfRel,
                                                        pDomain40Prog,
                                                        freqMHz);
                break;
            }
            default:
            {
                PMU_BREAKPOINT();
                break;
            }
        }
    }

    return bValidMatch;
}

/*!
 * @copydoc ClkVfRelClientFreqDeltaAdj
 */
FLCN_STATUS
clkVfRelClientFreqDeltaAdj
(
    CLK_VF_REL             *pVfRel,
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    LwU16                  *pFreqMHz
)
{
    CLK_VF_POINT_40        *pVfPoint40;
    LW2080_CTRL_CLK_VF_PAIR vfPair;
    LwS16       vfIdx;
    LwU8        lwrveIdx = LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI;
    FLCN_STATUS status   = FLCN_OK;

    // Short-circuit if OC is NOT allowed on this CLK_VF_REL entry
    if (!clkVfRelOVOCEnabled(pVfRel))
    {
        goto clkVfRelClientFreqDeltaAdj_exit;
    }

    // Sanity check.
    if ((LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx)) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx)))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkVfRelClientFreqDeltaAdj_exit;
    }

    // Iterate over this CLK_VF_REL's CLK_VF_POINTs:: MAX -> MIN
    for (vfIdx =  clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx);
         vfIdx >= clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx);
         vfIdx--)
    {
        pVfPoint40 = (CLK_VF_POINT_40 *)CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx, vfIdx);
        if (pVfPoint40 == NULL)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            PMU_BREAKPOINT();
            goto clkVfRelClientFreqDeltaAdj_exit;
        }

        // Retrieve the base frequency tuple.
        status = clkVfPoint40Accessor(pVfPoint40,                       // pVfPoint40
                                    pDomain40Prog,                      // pDomain40Prog
                                    pVfRel,                             // pVfRel
                                    lwrveIdx,                           // lwrveIdx
                                    LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,   // voltageType
                                    LW_FALSE,                           // bOffseted
                                    &vfPair);                           // pVfPair
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfRelClientFreqDeltaAdj_exit;
        }

        // Find the closest VF point matching input frequency.
        if (vfPair.freqMHz <= *pFreqMHz)
        {
            LwU16 baseFreqMHz = vfPair.freqMHz;

            // Retrieve the offseted frequency tuple.
            status = clkVfPoint40Accessor(pVfPoint40,                       // pVfPoint40
                                        pDomain40Prog,                      // pDomain40Prog
                                        pVfRel,                             // pVfRel
                                        lwrveIdx,                           // lwrveIdx
                                        LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,   // voltageType
                                        LW_TRUE,                            // bOffseted
                                        &vfPair);                           // pVfPair
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkVfRelClientFreqDeltaAdj_exit;
            }

            // If there is no frequency offset, short-circuit.
            if (baseFreqMHz == vfPair.freqMHz)
            {
                // Nothing to do.
                goto clkVfRelClientFreqDeltaAdj_exit;
            }

            // All Frequency based VF points.
            if ((BOARDOBJ_GET_TYPE(pVfRel) ==
                    LW2080_CTRL_CLK_CLK_VF_REL_TYPE_TABLE_FREQ) ||
                (BOARDOBJ_GET_TYPE(pVfRel) ==
                    LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_FREQ))
            {
                //
                // With Frequency based VF points, we MUST have an exact match
                // to client request in our base VF lwrve and we MUST select
                // the offseted value of that VF point. If we do not match it
                // then we could end up with VF point not available in supproted
                // PSTATE range.
                // For example, Let's consider DRAM CLK on GV100 where we have
                // PSTATE min = max = nom = 850 MHz. If client applied -50 MHz,
                // the only point available in VF lwrve will be 800 MHz frequency.
                // We must adjust CLK PROG offseted MAX and PSTATE MIN = MAX = NOM
                // = 800 MHz to keep the sane driver state.
                //
                if (baseFreqMHz != *pFreqMHz)
                {
                    status = FLCN_ERR_ILWALID_STATE;
                    PMU_HALT();
                    goto clkVfRelClientFreqDeltaAdj_exit;
                }

                (*pFreqMHz) = vfPair.freqMHz;
                goto clkVfRelClientFreqDeltaAdj_exit;
            }
            else
            {
                //
                // For voltage based VF points, we only take the postive
                // offets while adjusting the frequency for PSTATE tuple
                // as well as  CLK PROG MAX. In this case, Our VF lwrve
                // is determined by voltage as input param, we can always
                // support the MAX POR value as long as chip can allow the
                // voltage (reliablility voltage max) required to support
                // the offseted frequency point. So we only increase the max
                // freq with positive offset but we do not dicrease the max
                // freq if client apply negative offset. In case of negative
                // offset, we want to allow user to hit the POR max but with
                // higher voltage if chips can support it. If not, the voltage
                // max limit will cap the max allowed frequency but the
                // enumeration points till the max frequency will be still
                // available for arbiter to choose.
                //
                if (vfPair.freqMHz < baseFreqMHz)
                {
                    // Nothing to do.
                    goto clkVfRelClientFreqDeltaAdj_exit;
                }

                // If the exact match found, pick the offseted frequency value.
                if (baseFreqMHz == *pFreqMHz)
                {
                    (*pFreqMHz) = vfPair.freqMHz;
                    goto clkVfRelClientFreqDeltaAdj_exit;
                }
                else
                {
                    // Cache previous VF point offset. We know its positive.
                    LwU16 prevVFPointOffset = (vfPair.freqMHz - baseFreqMHz);
                    LwU16 offsettedFreqMHz  = (*pFreqMHz) + prevVFPointOffset;

                    LW2080_CTRL_CLK_VF_INPUT    inputFreq;
                    LW2080_CTRL_CLK_VF_OUTPUT   outputFreq;

                    //
                    // If offseted frequency value is greater then next VF point, cap
                    // it to next VF point frequency value in order to ensure monotonically
                    // increasing nature with voltage. Otherwise, select the max offset
                    // value of the two closest VF points as it is for free.
                    //
                    if (vfIdx != clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx))
                    {
                        pVfPoint40 = (CLK_VF_POINT_40 *)
                                        CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx, (vfIdx + 1U));
                        if (pVfPoint40 == NULL)
                        {
                            status = FLCN_ERR_ILWALID_INDEX;
                            PMU_BREAKPOINT();
                            goto clkVfRelClientFreqDeltaAdj_exit;
                        }

                        // Retrieve the offseted frequency tuple.
                        status = clkVfPoint40Accessor(pVfPoint40,                       // pVfPoint40
                                                    pDomain40Prog,                      // pDomain40Prog
                                                    pVfRel,                             // pVfRel
                                                    lwrveIdx,                           // lwrveIdx
                                                    LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,   // voltageType
                                                    LW_TRUE,                            // bOffseted
                                                    &vfPair);                           // pVfPair
                        if (status != FLCN_OK)
                        {
                            PMU_BREAKPOINT();
                            goto clkVfRelClientFreqDeltaAdj_exit;
                        }

                        if (offsettedFreqMHz >= vfPair.freqMHz)
                        {
                            (*pFreqMHz) = vfPair.freqMHz;
                            goto clkVfRelClientFreqDeltaAdj_exit;
                        }
                        else
                        {
                            LwU16 vfOffsetedFreqMHz = vfPair.freqMHz;

                            // Retrieve the base frequency tuple.
                            status = clkVfPoint40Accessor(pVfPoint40,                       // pVfPoint40
                                                        pDomain40Prog,                      // pDomain40Prog
                                                        pVfRel,                             // pVfRel
                                                        lwrveIdx,                           // lwrveIdx
                                                        LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,   // voltageType
                                                        LW_FALSE,                           // bOffseted
                                                        &vfPair);                           // pVfPair
                            if (status != FLCN_OK)
                            {
                                PMU_BREAKPOINT();
                                goto clkVfRelClientFreqDeltaAdj_exit;
                            }

                            if ((vfOffsetedFreqMHz > vfPair.freqMHz) &&
                                ((vfOffsetedFreqMHz - vfPair.freqMHz) > prevVFPointOffset))
                            {
                                offsettedFreqMHz = (*pFreqMHz) + (vfOffsetedFreqMHz - vfPair.freqMHz);
                            }
                        }
                    }

                    // Update the frequency with offset adjusted value and quantize down.
                    LW2080_CTRL_CLK_VF_INPUT_INIT(&inputFreq);
                    LW2080_CTRL_CLK_VF_OUTPUT_INIT(&outputFreq);

                    inputFreq.value = offsettedFreqMHz;
                    status = clkDomainProgFreqQuantize_40(
                                CLK_DOMAIN_PROG_GET_FROM_40_PROG(pDomain40Prog),    // pDomainProg
                                &inputFreq,                                         // pInputFreq
                                &outputFreq,                                        // pOutputFreq
                                LW_TRUE,                                            // bFloor
                                LW_FALSE);                                          // bBound
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                        goto clkVfRelClientFreqDeltaAdj_exit;
                    }
                    (*pFreqMHz) = (LwU16) outputFreq.value;
                    goto clkVfRelClientFreqDeltaAdj_exit;
                }
            }
            // Always exit...
            goto clkVfRelClientFreqDeltaAdj_exit;
        }
    }

clkVfRelClientFreqDeltaAdj_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelVfMaxFreqMHzGet
 */
FLCN_STATUS
clkVfRelVfMaxFreqMHzGet
(
    CLK_VF_REL             *pVfRel,
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    LwU16                  *pFreqMaxMHz
)
{
    CLK_VF_POINT_40        *pVfPoint40;
    LW2080_CTRL_CLK_VF_PAIR vfPair;
    LwU8            lwrveIdx = LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI;
    LwBoardObjIdx   vfIdx    = clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx);
    FLCN_STATUS     status;

    // Get VF Point pointer for max VF Point.
    pVfPoint40 = (CLK_VF_POINT_40 *)CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx, vfIdx);
    if (pVfPoint40 == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        PMU_BREAKPOINT();
        goto clkVfRelVfMaxFreqMHzGet_exit;
    }

    // Retrieve the offseted frequency tuple.
    status = clkVfPoint40Accessor(pVfPoint40,                       // pVfPoint40
                                pDomain40Prog,                      // pDomain40Prog
                                pVfRel,                             // pVfRel
                                lwrveIdx,                           // lwrveIdx
                                LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,   // voltageType
                                LW_TRUE,                            // bOffseted
                                &vfPair);                           // pVfPair
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfRelVfMaxFreqMHzGet_exit;
    }
    (*pFreqMaxMHz) = vfPair.freqMHz;

clkVfRelVfMaxFreqMHzGet_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelIsNoiseAware
 */
FLCN_STATUS
clkVfRelIsNoiseAware
(
    CLK_VF_REL *pVfRel
)
{
    return (BOARDOBJ_GET_TYPE(pVfRel) ==
            LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_VOLT);
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * CLK_VF_REL implementation to parse BOARDOBJGRP header.
 *
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
static FLCN_STATUS
s_clkVfRelIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_VF_REL_BOARDOBJGRP_SET_HEADER *pVfRelsSet =
        (RM_PMU_CLK_CLK_VF_REL_BOARDOBJGRP_SET_HEADER *)pHdrDesc;
    CLK_VF_RELS *pVfRels = (CLK_VF_RELS *)pBObjGrp;
    FLCN_STATUS status;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkVfRelIfaceModel10SetHeader_exit;
    }

    // Copy global CLK_VF_RELS state.
    pVfRels->secondaryEntryCount = pVfRelsSet->secondaryEntryCount;
    pVfRels->vfEntryCountSec = pVfRelsSet->vfEntryCountSec;

s_clkVfRelIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * CLK_VF_REL implementation to parse BOARDOBJ entry.
 *
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_clkVfRelIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    switch (pBoardObjSet->type)
    {
        case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_VOLT:
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_REL))
        {
            return clkVfRelGrpIfaceModel10ObjSet_RATIO_VOLT(pModel10, ppBoardObj,
                        sizeof(CLK_VF_REL_RATIO_VOLT), pBoardObjSet);
        }
        break;
        case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_FREQ:
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_REL))
        {
            return clkVfRelIfaceModel10Set_RATIO_FREQ(pModel10, ppBoardObj,
                        sizeof(CLK_VF_REL_RATIO), pBoardObjSet);
        }
        break;
        case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_TABLE_FREQ:
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_REL))
        {
            return clkVfRelIfaceModel10Set_TABLE_FREQ(pModel10, ppBoardObj,
                        sizeof(CLK_VF_REL_TABLE), pBoardObjSet);
        }
        break;
        default:
        {
            break;
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
static FLCN_STATUS
s_clkVfRelIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS status = FLCN_OK;

    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_FREQ:
        case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_VOLT:
        case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_TABLE_FREQ:
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_REL))
        {
            RM_PMU_CLK_CLK_VF_REL_BOARDOBJ_GET_STATUS *pVfRelStatus =
                (RM_PMU_CLK_CLK_VF_REL_BOARDOBJ_GET_STATUS *)pBuf;
            CLK_VF_REL *pVfRel = (CLK_VF_REL *)pBoardObj;

            status = boardObjIfaceModel10GetStatus(pModel10, pBuf);
            if (status != FLCN_OK)
            {
                goto s_clkVfRelIfaceModel10GetStatus_exit;
            }

            // Copy-out the VF_REL specific params.
            pVfRelStatus->offsettedFreqMaxMHz = pVfRel->offsettedFreqMaxMHz;
        }
        break;
        default:
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NOT_SUPPORTED;
            goto s_clkVfRelIfaceModel10GetStatus_exit;
        }
    }

 s_clkVfRelIfaceModel10GetStatus_exit:
    return status;
}
