/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file lib_avg.c
 *
 * @brief Module managing all state related to the avg math library structure.
 */

/* ------------------------ Includes --------------------------------------- */
#include "lib/lib_avg.h"
#include "pmu_objclk.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
static COUNTER *_counterGet(LwU8 clientId, LwU8 index)
    GCC_ATTRIB_SECTION("imem_libAvg", "_counterGet");

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * Allocates memory to store the counters
 *
 * @return FLCN_OK
 *       Computed average successfully, result updated in pAvg.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      If client is not supported
 * @return FLCN_ERR_ILLEGAL_OPERATION
 *      If memory is already allocated
 * @return FLCN_ERR_NO_FREE_MEM
 *      If memory allocation fails
 */
FLCN_STATUS
swCounterAvgInit()
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        i;

    for (i = 1; i < COUNTER_AVG_CLIENT_ID_MAX; i++)
    {
        if (Clk.clkSwCounterAvg[i].pCounter != NULL)
        {
            status = FLCN_ERR_ILLEGAL_OPERATION;
            PMU_BREAKPOINT();
            goto swCounterAvgInit_exit;
        }

        switch (i)
        {
            case COUNTER_AVG_CLIENT_ID_VOLT:
            {
                Clk.clkSwCounterAvg[i].pCounter = lwosCallocType(OVL_INDEX_DMEM(libAvg),
                    LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS, COUNTER);
                break;
            }
        }

        if (Clk.clkSwCounterAvg[i].pCounter == NULL)
        {
            status = FLCN_ERR_NO_FREE_MEM;
            PMU_BREAKPOINT();
            goto swCounterAvgInit_exit;
        }
    }

swCounterAvgInit_exit:
    return status;
}

/*!
 * Client interface to compute the average. Computes the
 * average as unit product difference / time difference, where the difference
 * is computed between the current sample and the sample provided by the client.
 *
 * It also updates the client's passed sample with the current sample after
 * computation.
 *
 * @param[in]     clientId
 *      Client Id @ref COUNTER_AVG_CLIENT_ID_<XYZ> 
 * @param[in]     index
 *      Index passed by the client
 * @param[in/out] pSample
 *      Pointer to the client's sample against which the current sample will be
 *      differenced to compute average. After computation, this will be updated
 *      to the current sample.
 * @param[out]    pAvg
 *      Pointer to memory to return avg computed to the caller.
 *
 * @return FLCN_OK
 *       Computed average successfully, result updated in pAvg.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      Invalid index or sample provided by the client.
 * @return FLCN_ERR_ILWALID_ARGUENT
 *      If pAvg is not valid pointer
 * @return other
 *      Any other errors while computing average
 */
FLCN_STATUS
swCounterAvgCompute
(
    LwU8               clientId,
    LwU8               index,
    COUNTER_SAMPLE    *pSample,
    LwU32             *pAvg
)
{
    FLCN_STATUS status   = FLCN_OK;
    LwU64       average  = 0;
    COUNTER    *pCounter = NULL;
    LwU64       timeDiffns;
    LwU64       unitProdDiff;

    if (!COUNTER_AVG_CLIENT_IS_VALID(clientId))
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        PMU_BREAKPOINT();
        goto swCounterAvgCompute_exit;
    }

    if (pAvg == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto swCounterAvgCompute_exit;
    }

    pCounter = _counterGet(clientId, index);
    if (pCounter == NULL)
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        PMU_BREAKPOINT();
        goto swCounterAvgCompute_exit;
    }

    // Update cached time and unit product using cached value.
    status = swCounterAvgUpdate(clientId, index, pCounter->prevTarget);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto swCounterAvgCompute_exit;
    }

    // Compute time difference
    lw64Sub(&timeDiffns, &pCounter->prevSample.timens, &pSample->timens);

    // Compute unit prod difference
    if (pCounter->prevSample.unitProduct < pSample->unitProduct)
    {
        // Handle overflow case
        unitProdDiff = LW_U64_MAX;
        lw64Sub(&unitProdDiff, &unitProdDiff, &pSample->unitProduct);
        lw64Add(&unitProdDiff, &unitProdDiff, &pCounter->prevSample.unitProduct);
    }
    else
    {
        lw64Sub(&unitProdDiff, &pCounter->prevSample.unitProduct, &pSample->unitProduct);
    }

    // Compute average
    if (timeDiffns != 0)
    {
        lwU64Div(&average, &unitProdDiff, &timeDiffns);
    }
    else
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        PMU_BREAKPOINT();
        goto swCounterAvgCompute_exit;
    }

    // Update client's sample with the new one.
    pSample->unitProduct = pCounter->prevSample.unitProduct;
    pSample->timens      = pCounter->prevSample.timens;

    *pAvg = (LwU32)average;

swCounterAvgCompute_exit:
    return status;
}

/*!
 * Updates the counter sample based on the previous requested counter value
 * and caches the new one for use in the next cycle.
 *
 * @param[in]   clientId
 *      Client Id @ref COUNTER_AVG_CLIENT_ID_<XYZ>
 * @param[in]   index
 *      Index passed by the client
 * @param[in]   target
 *      New requested target
 *
 * @return FLCN_OK
 *      Successfully updates the counter sample and cached value.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      Unsupported clientId or index passed.
 */
FLCN_STATUS
swCounterAvgUpdate
(
    LwU8  clientId,
    LwU8  index,
    LwU32 target
)
{
    FLCN_TIMESTAMP  timeNowns;
    LwU64           timeDiffns;
    LwU64           tempTarget;
    LwU64           tempUnitProd;
    COUNTER        *pCounter = NULL;
    FLCN_STATUS     status   = FLCN_OK;

    if (!COUNTER_AVG_CLIENT_IS_VALID(clientId))
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        PMU_BREAKPOINT();
        goto swCounterAvgUpdate_exit;
    }

    pCounter = _counterGet(clientId, index);
    if (pCounter == NULL)
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        PMU_BREAKPOINT();
        goto swCounterAvgUpdate_exit;
    }

    // Elevate to critical to synchronize average counter state.
    appTaskCriticalEnter();
    {
        // Get current timestamp.
        osPTimerTimeNsLwrrentGet(&timeNowns);

        // Update the product only if previous sample is stored.
        if (pCounter->prevTarget != 0)
        {
            // Compute elapsed product since last update.
            lw64Sub(&timeDiffns, &(timeNowns.data), &pCounter->prevSample.timens);
            tempTarget = pCounter->prevTarget;
            lwU64Mul(&tempUnitProd, &timeDiffns, &tempTarget);

            // Update product by adding to elapsed product.
            lw64Add(&pCounter->prevSample.unitProduct, &pCounter->prevSample.unitProduct, &tempUnitProd);
        }

        // Update previous values to the new ones.
        pCounter->prevSample.timens = timeNowns.data;
        pCounter->prevTarget = target;
    }
    appTaskCriticalExit();

swCounterAvgUpdate_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Helper function to get the pointer to counter object based on the
 * client's Id and index passed.
 *
 * @param[in]   client
 *      Client Id @ref COUNTER_AVG_CLIENT_ID_<XYZ>
 * @param[in]   index
 *      Index passed by the client
 *
 * @return Pointer to counter object
 * @return NULL
 *      Unsupported client Id or index passed.
 */
COUNTER *
_counterGet
(
    LwU8 clientId,
    LwU8 index
)
{
    switch (clientId)
    {
        case COUNTER_AVG_CLIENT_ID_VOLT:
        {
            if (index < LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS)
            {
                return &Clk.clkSwCounterAvg[clientId].pCounter[index];
            }
            break;
        }
        default:
        {
            break;
        }
    }

    PMU_BREAKPOINT();
    return NULL;
}
