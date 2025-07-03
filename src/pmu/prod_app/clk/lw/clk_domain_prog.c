/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"

#include "g_pmurpc.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_DOMAIN_PROG class constructor.
 *
 * @copydoc BoardObjInterfaceConstruct
 */
FLCN_STATUS
clkDomainConstruct_PROG
(
    BOARDOBJGRP                *pBObjGrp,
    BOARDOBJ_INTERFACE         *pInterface,
    RM_PMU_BOARDOBJ_INTERFACE  *pInterfaceDesc,
    INTERFACE_VIRTUAL_TABLE    *pVirtualTable
)
{
    RM_PMU_CLK_CLK_DOMAIN_PROG_BOARDOBJ_SET *pSet =
        (RM_PMU_CLK_CLK_DOMAIN_PROG_BOARDOBJ_SET *)pInterfaceDesc;
    CLK_DOMAIN_PROG    *pDomainProg;
    FLCN_STATUS         status;

    status = boardObjInterfaceConstruct(pBObjGrp, pInterface, pInterfaceDesc, pVirtualTable);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainConstruct_PROG_exit;
    }
    pDomainProg = (CLK_DOMAIN_PROG *)pInterface;

    (void)pSet;
    (void)pDomainProg;

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_PROG_VOLT_RAIL_VMIN_MASK)
    // Copy the CLK_DOMAIN_30_PRIMARY-specific data.
    boardObjGrpMaskInit_E32(&(pDomainProg->voltRailVminMask));
    status = boardObjGrpMaskImport_E32(&(pDomainProg->voltRailVminMask),
                                       &(pSet->voltRailVminMask));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainConstruct_PROG_exit;
    }
#endif

clkDomainConstruct_PROG_exit:
    return status;
}


/*!
 * @copydoc ClkDomainProgIsSecVFLwrvesEnabled
 */
FLCN_STATUS
clkDomainProgIsSecVFLwrvesEnabled
(
    CLK_DOMAIN_PROG                    *pDomainProg,
    LwBool                             *pBIsSecVFLwrvesEnabled
)
{
    CLK_DOMAIN *pDomain =
        (CLK_DOMAIN *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    FLCN_STATUS status  = FLCN_ERR_NOT_SUPPORTED;

    if ((pBIsSecVFLwrvesEnabled == NULL) ||
        (pDomain == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkDomainProgIsSecVFLwrvesEnabled_exit;
    }

    *pBIsSecVFLwrvesEnabled = LW_FALSE;

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
        {
            // _30_PROG type do not support secondary VF lwrves
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_30))
            {
                status = clkDomainProgIsSecVFLwrvesEnabled_30(pDomainProg,
                            pBIsSecVFLwrvesEnabled);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_BASE_35))
            {
                status = clkDomainProgIsSecVFLwrvesEnabled_35(pDomainProg,
                            pBIsSecVFLwrvesEnabled);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
            {
                status = clkDomainProgIsSecVFLwrvesEnabled_40(pDomainProg,
                            pBIsSecVFLwrvesEnabled);
            }
            break;
        }
        default:
        {
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

clkDomainProgIsSecVFLwrvesEnabled_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgIsSecVFLwrvesEnabled
 */
FLCN_STATUS
clkDomainProgIsSecVFLwrvesEnabledByApiDomain
(
    LwU32   apiDomain,
    LwBool *pBIsSecVFLwrvesEnabled
)
{
    CLK_DOMAIN      *pDomain        = NULL;
    CLK_DOMAIN_PROG *pDomainProg    = NULL;
    FLCN_STATUS      status         = FLCN_ERR_NOT_SUPPORTED;

    *pBIsSecVFLwrvesEnabled         = LW_FALSE;

    pDomain = clkDomainsGetByApiDomain(apiDomain);
    if (pDomain == NULL)
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto clkDomainProgIsSecVFLwrvesEnabledByApiDomain_exit;
    }

    if (clkDomainIsFixed(pDomain))
    {
        status = FLCN_OK;
        goto clkDomainProgIsSecVFLwrvesEnabledByApiDomain_exit;
    }

    pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
    if (pDomainProg == NULL)
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto clkDomainProgIsSecVFLwrvesEnabledByApiDomain_exit;
    }

    status = clkDomainProgIsSecVFLwrvesEnabled(pDomainProg, pBIsSecVFLwrvesEnabled);

clkDomainProgIsSecVFLwrvesEnabledByApiDomain_exit:
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
    return status;
}

/*!
 * @copydoc ClkDomainProgVoltToFreq
 */
FLCN_STATUS
clkDomainProgVoltToFreq
(
    CLK_DOMAIN_PROG                    *pDomainProg,
    LwU8                                voltRailIdx,
    LwU8                                voltageType,
    LW2080_CTRL_CLK_VF_INPUT           *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT          *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState
)
{
    CLK_DOMAIN *pDomain =
        (CLK_DOMAIN *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    FLCN_STATUS status  = FLCN_ERR_NOT_SUPPORTED;

    if (!VOLT_RAIL_INDEX_IS_VALID(voltRailIdx))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkDomainProgVoltToFreq_exit;
    }

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_BASE_3X))
            {
                status = clkDomainProgVoltToFreq_3X(pDomainProg,
                            voltRailIdx, voltageType, pInput, pOutput,
                            pVfIterState);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
            {
                status = clkDomainProgVoltToFreq_40(pDomainProg,
                            voltRailIdx, voltageType, pInput, pOutput, NULL);
            }
            break;
        }
        default:
        {
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

clkDomainProgVoltToFreq_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgFreqToVolt
 */
FLCN_STATUS
clkDomainProgFreqToVolt
(
    CLK_DOMAIN_PROG                    *pDomainProg,
    LwU8                                voltRailIdx,
    LwU8                                voltageType,
    LW2080_CTRL_CLK_VF_INPUT           *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT          *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState
)
{
    CLK_DOMAIN *pDomain =
        (CLK_DOMAIN *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    FLCN_STATUS status  = FLCN_ERR_NOT_SUPPORTED;

    if (!VOLT_RAIL_INDEX_IS_VALID(voltRailIdx))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkDomainProgFreqToVolt_exit;
    }

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_BASE_3X))
            {
                status = clkDomainProgFreqToVolt_3X(pDomainProg,
                            voltRailIdx, voltageType, pInput, pOutput,
                            pVfIterState);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
            {
                status = clkDomainProgFreqToVolt_40(pDomainProg,
                            voltRailIdx, voltageType, pInput, pOutput, NULL);
            }
            break;
        }
        default:
        {
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

clkDomainProgFreqToVolt_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgVoltToFreqTuple
 */
FLCN_STATUS
clkDomainProgVoltToFreqTuple
(
    CLK_DOMAIN_PROG                                *pDomainProg,
    LwU8                                            voltRailIdx,
    LwU8                                            voltageType,
    LW2080_CTRL_CLK_VF_INPUT                       *pInput,
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE    *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE             *pVfIterState
)
{
    CLK_DOMAIN *pDomain =
        (CLK_DOMAIN *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    FLCN_STATUS status  = FLCN_ERR_NOT_SUPPORTED;

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_BASE_3X))
            {
                status = clkDomainProgVoltToFreqTuple_35(pDomainProg,
                            voltRailIdx, voltageType, pInput, pOutput,
                            pVfIterState);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
            {
                status = clkDomainProgVoltToFreqTuple_40(pDomainProg,
                            voltRailIdx, voltageType, pInput, pOutput, pVfIterState);
            }
            break;
        }
        default:
        {
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @copydoc ClkDomainProgVfMaxFreqMHzGet
 */
FLCN_STATUS
clkDomainProgVfMaxFreqMHzGet
(
    CLK_DOMAIN_PROG    *pDomainProg,
    LwU32              *pFreqMaxMHz
)
{
    CLK_DOMAIN *pDomain =
        (CLK_DOMAIN *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    FLCN_STATUS status  = FLCN_ERR_NOT_SUPPORTED;

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_EXTENDED_3X))
            {
                status = clkDomainProgMaxFreqMHzGet_3X(pDomainProg, pFreqMaxMHz);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
            {
                status = clkDomainProgVfMaxFreqMHzGet_40(pDomainProg, pFreqMaxMHz);
            }
            break;
        }
        default:
        {
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @copydoc ClkDomainProgGetFreqMHzByIndex
 */
LwU16
clkDomainProgGetFreqMHzByIndex
(
    CLK_DOMAIN_PROG *pDomainProg,
    LwU16            idx
)
{
    CLK_DOMAIN *pDomain =
        (CLK_DOMAIN *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    LwU16       freqMHz = LW_U16_MAX;
    FLCN_STATUS status;
    LwBool      bIsNoiseAware = LW_FALSE;

    PMU_ASSERT_OK_OR_GOTO(status,
        clkDomainProgIsNoiseAware(pDomainProg, &bIsNoiseAware),
        clkDomainProgGetFreqMHzByIndex_exit);
    if (!bIsNoiseAware)
    {
        PMU_BREAKPOINT();
        goto clkDomainProgGetFreqMHzByIndex_exit;
    }

    // Callwlate the frequency value from input frequency index.
    if (FLCN_OK != clkNafllGetFreqMHzByIndex(
                    clkDomainApiDomainGet(pDomain),
                    idx,
                    &freqMHz))
    {
        PMU_BREAKPOINT();
        freqMHz = LW_U16_MAX;
        goto clkDomainProgGetFreqMHzByIndex_exit;
    }

clkDomainProgGetFreqMHzByIndex_exit:

    // Sanity check the output.
    if (freqMHz == LW_U16_MAX)
    {
        PMU_BREAKPOINT();
    }

    return freqMHz;
}

/*!
 * @copydoc ClkDomainProgGetIndexByFreqMHz
 */
LwU16
clkDomainProgGetIndexByFreqMHz
(
    CLK_DOMAIN_PROG    *pDomainProg,
    LwU16               freqMHz,
    LwBool              bFloor
)
{
    CLK_DOMAIN *pDomain =
        (CLK_DOMAIN *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    LwU16       freqIdx = LW_U16_MAX;
    FLCN_STATUS status;
    LwBool      bIsNoiseAware = LW_FALSE;

    PMU_ASSERT_OK_OR_GOTO(status,
        clkDomainProgIsNoiseAware(pDomainProg, &bIsNoiseAware),
        clkDomainProgGetIndexByFreqMHz_exit);
    if (!bIsNoiseAware)
    {
        PMU_BREAKPOINT();
        goto clkDomainProgGetIndexByFreqMHz_exit;
    }

    // Callwlate the frequency index from input frequency value.
    if (FLCN_OK != clkNafllGetIndexByFreqMHz(
                    clkDomainApiDomainGet(pDomain),
                    freqMHz,
                    bFloor,
                    &freqIdx))
    {
        PMU_BREAKPOINT();
        freqIdx = LW_U16_MAX;
        goto clkDomainProgGetIndexByFreqMHz_exit;
    }

clkDomainProgGetIndexByFreqMHz_exit:

    // Sanity check the output.
    if (freqIdx == LW_U16_MAX)
    {
        PMU_BREAKPOINT();
    }

    return freqIdx;
}

/*!
 * @copydoc ClkDomainProgFreqEnumIterate
 */
FLCN_STATUS
clkDomainProgFreqEnumIterate
(
    CLK_DOMAIN_PROG    *pDomainProg,
    LwU16              *pFreqMHz
)
{
    CLK_DOMAIN *pDomain =
        (CLK_DOMAIN *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    FLCN_STATUS status  = FLCN_ERR_NOT_SUPPORTED;

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_EXTENDED_3X))
            {
                status = clkDomainProgFreqsIterate_3X(pDomainProg, pFreqMHz);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
            {
                status = clkDomainProgFreqEnumIterate_40(pDomainProg, pFreqMHz);
            }
            break;
        }
        default:
        {
            break;
        }
    }

    //
    // Expected return status "FLCN_ERR_ITERATION_END"
    // Client will handle status.
    //

    return status;
}

/*!
 * @copydoc ClkDomainProgFreqQuantize
 */
FLCN_STATUS
clkDomainProgFreqQuantize
(
    CLK_DOMAIN_PROG            *pDomainProg,
    LW2080_CTRL_CLK_VF_INPUT   *pInputFreq,
    LW2080_CTRL_CLK_VF_OUTPUT  *pOutputFreq,
    LwBool                      bFloor,
    LwBool                      bBound
)
{
    CLK_DOMAIN *pDomain =
        (CLK_DOMAIN *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    FLCN_STATUS status  = FLCN_ERR_NOT_SUPPORTED;

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_EXTENDED_3X))
            {
                //
                // bBound is un-used and always true on 3X.
                // Always adjust with frequency OCOV.
                //
                status = clkDomainProgFreqQuantize_3X(pDomainProg,
                            pInputFreq, pOutputFreq, bFloor, LW_TRUE);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
            {
                status = clkDomainProgFreqQuantize_40(pDomainProg,
                            pInputFreq, pOutputFreq, bFloor, bBound);
            }
            break;
        }
        default:
        {
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @copydoc ClkDomainProgClientFreqDeltaAdj
 */
FLCN_STATUS
clkDomainProgClientFreqDeltaAdj
(
    CLK_DOMAIN_PROG    *pDomainProg,
    LwU16              *pFreqMHz
)
{
    CLK_DOMAIN *pDomain =
        (CLK_DOMAIN *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    FLCN_STATUS status  = FLCN_ERR_NOT_SUPPORTED;

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_EXTENDED_3X))
            {
                status = clkDomainProgClientFreqDeltaAdj_3X(pDomainProg, pFreqMHz);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
            {
                status = clkDomainProgClientFreqDeltaAdj_40(pDomainProg, pFreqMHz);
            }
            break;
        }
        default:
        {
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @copydoc ClkDomainProgFactoryDeltaAdj
 */
FLCN_STATUS
clkDomainProgFactoryDeltaAdj
(
    CLK_DOMAIN_PROG    *pDomainProg,
    LwU16              *pFreqMHz
)
{
    CLK_DOMAIN *pDomain =
        (CLK_DOMAIN *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    FLCN_STATUS status  = FLCN_ERR_NOT_SUPPORTED;

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_EXTENDED_3X))
            {
                status = clkDomainProgFactoryDeltaAdj_3X(pDomainProg, pFreqMHz);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
            {
                status = clkDomainProgFactoryDeltaAdj_40(pDomainProg, pFreqMHz);
            }
            break;
        }
        default:
        {
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @copydoc ClkDomainProgIsFreqMHzNoiseAware
 */
LwBool
clkDomainProgIsFreqMHzNoiseAware
(
    CLK_DOMAIN_PROG    *pDomainProg,
    LwU32               freqMHz
)
{
    CLK_DOMAIN *pDomain     =
        (CLK_DOMAIN *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    LwBool      bNoiseAware = LW_FALSE;
    FLCN_STATUS status      = FLCN_ERR_NOT_SUPPORTED;

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_EXTENDED_3X))
            {
                bNoiseAware = clkDomainProgIsFreqMHzNoiseAware_3X(pDomainProg, freqMHz);
                status = FLCN_OK;
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
            {
                bNoiseAware = clkDomainProgIsFreqMHzNoiseAware_40(pDomainProg, freqMHz);
                status = FLCN_OK;
            }
            break;
        }
        default:
        {
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return bNoiseAware;
}

/*!
 * @copydoc ClkDomainProgIsNoiseAware
 */
FLCN_STATUS
clkDomainProgIsNoiseAware_IMPL
(
    CLK_DOMAIN_PROG  *pDomainProg,
    LwBool           *pbIsNoiseAware
)
{
    CLK_DOMAIN *pDomain =
        (CLK_DOMAIN *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    FLCN_STATUS status  = FLCN_ERR_NOT_SUPPORTED;

    *pbIsNoiseAware = LW_FALSE;

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                clkDomainProgIsNoiseAware_3X(pDomainProg, pbIsNoiseAware),
                clkDomainProgIsNoiseAware_exit);
            break;
        }
        default:
        {
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

clkDomainProgIsNoiseAware_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgPreVoltOrderingIndexGet
 */
LwU8
clkDomainProgPreVoltOrderingIndexGet
(
    CLK_DOMAIN_PROG    *pDomainProg
)
{
    CLK_DOMAIN *pDomain =
        (CLK_DOMAIN *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    LwU8        idx     = LW2080_CTRL_CLK_CLK_DOMAIN_3X_PROG_ORDERING_INDEX_ILWALID;

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_BASE_35))
            {
                idx = clkDomainProgPreVoltOrderingIndexGet_35(pDomainProg);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
            {
                idx = clkDomainProgPreVoltOrderingIndexGet_40(pDomainProg);
            }
            break;
        }
        default:
        {
            break;
        }
    }

    if (idx == LW2080_CTRL_CLK_CLK_DOMAIN_3X_PROG_ORDERING_INDEX_ILWALID)
    {
        PMU_BREAKPOINT();
    }

    return idx;
}

/*!
 * @copydoc ClkDomainProgPostVoltOrderingIndexGet
 */
LwU8
clkDomainProgPostVoltOrderingIndexGet
(
    CLK_DOMAIN_PROG    *pDomainProg
)
{
    CLK_DOMAIN *pDomain =
        (CLK_DOMAIN *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    LwU8        idx     = LW2080_CTRL_CLK_CLK_DOMAIN_3X_PROG_ORDERING_INDEX_ILWALID;

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_BASE_35))
            {
                idx = clkDomainProgPostVoltOrderingIndexGet_35(pDomainProg);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
            {
                idx = clkDomainProgPostVoltOrderingIndexGet_40(pDomainProg);
            }
            break;
        }
        default:
        {
            break;
        }
    }

    if (idx == LW2080_CTRL_CLK_CLK_DOMAIN_3X_PROG_ORDERING_INDEX_ILWALID)
    {
        PMU_BREAKPOINT();
    }

    return idx;
}

/*!
 * @copydoc ClkDomainProgVoltRailIdxGet
 */
LwU8
clkDomainProgVoltRailIdxGet
(
    CLK_DOMAIN_PROG    *pDomainProg
)
{
    CLK_DOMAIN *pDomain =
        (CLK_DOMAIN *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    LwU8        idx     = LW2080_CTRL_BOARDOBJ_IDX_ILWALID_8BIT;

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
            {
                idx = clkDomainProgVoltRailIdxGet_40(pDomainProg);
            }
            break;
        }
        default:
        {
            break;
        }
    }

    if (idx == LW2080_CTRL_BOARDOBJ_IDX_ILWALID_8BIT)
    {
        PMU_BREAKPOINT();
    }

    return idx;
}

/*!
 * @copydoc ClkDomainProgFbvddDataValid
 */
FLCN_STATUS
clkDomainProgFbvddDataValid
(
    CLK_DOMAIN_PROG    *pDomainProg,
    LwBool             *pbValid
)
{
    FLCN_STATUS status;
    CLK_DOMAIN *pDomain;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomainProg != NULL) &&
        (pbValid != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        clkDomainProgFbvddDataValid_exit);

    pDomain = (CLK_DOMAIN *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomain != NULL),
        FLCN_ERR_ILWALID_STATE,
        clkDomainProgFbvddDataValid_exit);

    // Call type-specific implemenation.
    status = FLCN_ERR_NOT_SUPPORTED;
    switch (BOARDOBJ_GET_TYPE(pDomain))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
            {
                status = clkDomainProgFbvddDataValid_40(
                    pDomainProg, pbValid);
            }
            break;
        }
        default:
        {
            // Default to the _SUPER implementation for other types
            status = clkDomainProgFbvddDataValid_SUPER(
                    pDomainProg, pbValid);
            break;
        }
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        status,
        clkDomainProgFbvddDataValid_exit);

clkDomainProgFbvddDataValid_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgFbvddPwrAdjust
 */
FLCN_STATUS
clkDomainProgFbvddPwrAdjust
(
    CLK_DOMAIN_PROG    *pDomainProg,
    LwU32              *pFbPwrmW,
    LwBool              bFromReference
)
{
    FLCN_STATUS status;
    CLK_DOMAIN *pDomain =
        (CLK_DOMAIN *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);

    // Call type-specific implemenation.
    status = FLCN_ERR_NOT_SUPPORTED;
    switch (BOARDOBJ_GET_TYPE(pDomain))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
            {
                status = clkDomainProgFbvddPwrAdjust_40(
                    pDomainProg, pFbPwrmW, bFromReference);
            }
            break;
        }
        default:
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
        }
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        status,
        clkDomainProgFbvddPwrAdjust_exit);

clkDomainProgFbvddPwrAdjust_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgFbvddFreqToVolt
 */
FLCN_STATUS
clkDomainProgFbvddFreqToVolt
(
    CLK_DOMAIN_PROG                *pDomainProg,
    const LW2080_CTRL_CLK_VF_INPUT *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT      *pOutput
)
{
    FLCN_STATUS status;
    CLK_DOMAIN *pDomain =
        (CLK_DOMAIN *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);

    // Call type-specific implemenation.
    status = FLCN_ERR_NOT_SUPPORTED;
    switch (BOARDOBJ_GET_TYPE(pDomain))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
            {
                status = clkDomainProgFbvddFreqToVolt_40(
                    pDomainProg, pInput, pOutput);
            }
            break;
        }
        default:
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
        }
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        status,
        clkDomainProgFbvddFreqToVolt_exit);

clkDomainProgFbvddFreqToVolt_exit:
    return status;
}

/*!
 * @copydoc ClkDomainsProgFreqPropagate
 */
FLCN_STATUS
clkDomainsProgFreqPropagate
(
    CLK_DOMAIN_PROG_FREQ_PROP_IN_OUT_TUPLE *pFreqPropTuple,
    LwBoardObjIdx                           clkPropTopIdx
)
{
    CLK_DOMAIN     *pDomainSrc;
    LwBoardObjIdx   srcIdx;
    FLCN_STATUS     status  = FLCN_ERR_NOT_SUPPORTED;

    // Sanity check input.

    //
    // Ensure the input and output clock domain mask is subset of programmable
    // clock domain mask.
    //
    if (!boardObjGrpMaskIsSubset((pFreqPropTuple->pDomainsMaskIn),
                                 &(Clk.domains.progDomainsMask)))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkDomainsProgFreqPropagate_exit;
    }

    if (!boardObjGrpMaskIsSubset((pFreqPropTuple->pDomainsMaskOut),
                                 &(Clk.domains.progDomainsMask)))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkDomainsProgFreqPropagate_exit;
    }

    // Ensure all input frequency values are non zero.
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomainSrc, srcIdx, &pFreqPropTuple->pDomainsMaskIn->super)
    {
        // Init to source frequency.
        if (pFreqPropTuple->freqMHz[srcIdx] == 0U)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto clkDomainsProgFreqPropagate_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    // Call type-specific implemenation.
    switch (clkDomainsVersionGet())
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_VERSION_40:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
            {
                status = clkDomainsProgFreqPropagate_40(pFreqPropTuple, clkPropTopIdx);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_VERSION_35:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_EXTENDED_35))
            {
                status = clkDomainsProgFreqPropagate_35(pFreqPropTuple, clkPropTopIdx);
            }
            break;
        }
        default:
        {
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainsProgFreqPropagate_exit;
    }

clkDomainsProgFreqPropagate_exit:
    return status;
}

/*!
 * @brief   Exelwtes generic CLK_DOMAIN_PROG_VOLT_TO_FREQ RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_CLK_CLK_DOMAIN_PROG_VOLT_TO_FREQ
 */
FLCN_STATUS
pmuRpcClkClkDomainProgVoltToFreq
(
    RM_PMU_RPC_STRUCT_CLK_CLK_DOMAIN_PROG_VOLT_TO_FREQ *pParams
)
{
    CLK_DOMAIN         *pDomain =
        (CLK_DOMAIN *)BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, pParams->clkDomainIdx);
    CLK_DOMAIN_PROG    *pDomainProg;
    FLCN_STATUS         status;

    // Sanity check the input clock domain index.
    if ((pDomain == NULL) ||
        (BOARDOBJ_GET_TYPE(pDomain) == LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto pmuRpcClkClkDomainProgVoltToFreq_exit;
    }

    pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
    if (pDomainProg == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto pmuRpcClkClkDomainProgVoltToFreq_exit;
    }

    // Always set default flag to true on external client.
    pParams->input.flags = FLD_SET_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS,
                            _VF_POINT_SET_DEFAULT, _YES, pParams->input.flags);
    status = clkDomainProgVoltToFreq(pDomainProg,
                                     pParams->voltRailIdx,
                                     pParams->voltageType,
                                     &pParams->input,
                                     &pParams->output,
                                     NULL);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pmuRpcClkClkDomainProgVoltToFreq_exit;
    }

pmuRpcClkClkDomainProgVoltToFreq_exit:
    return status;
}

/*!
 * @brief   Exelwtes generic CLK_DOMAIN_PROG_FREQ_TO_VOLT RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_CLK_CLK_DOMAIN_PROG_FREQ_TO_VOLT
 */
FLCN_STATUS
pmuRpcClkClkDomainProgFreqToVolt
(
    RM_PMU_RPC_STRUCT_CLK_CLK_DOMAIN_PROG_FREQ_TO_VOLT *pParams
)
{
    CLK_DOMAIN         *pDomain =
        (CLK_DOMAIN *)BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, pParams->clkDomainIdx);
    CLK_DOMAIN_PROG    *pDomainProg;
    FLCN_STATUS         status;

    // Sanity check the input clock domain index.
    if ((pDomain == NULL) ||
        (BOARDOBJ_GET_TYPE(pDomain) == LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto pmuRpcClkClkDomainProgFreqToVolt_exit;
    }

    pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
    if (pDomainProg == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto pmuRpcClkClkDomainProgFreqToVolt_exit;
    }

    // Always set default flag to true on external client.
    pParams->input.flags = FLD_SET_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS,
                            _VF_POINT_SET_DEFAULT, _YES, pParams->input.flags);
    status = clkDomainProgFreqToVolt(pDomainProg,
                                     pParams->voltRailIdx,
                                     pParams->voltageType,
                                     &pParams->input,
                                     &pParams->output,
                                     NULL);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pmuRpcClkClkDomainProgFreqToVolt_exit;
    }

pmuRpcClkClkDomainProgFreqToVolt_exit:
    return status;
}

/*!
 * @brief   Exelwtes generic CLK_DOMAIN_PROG_FREQ_QUANTIZE RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_CLK_CLK_DOMAIN_PROG_FREQ_QUANTIZE
 */
FLCN_STATUS
pmuRpcClkClkDomainProgFreqQuantize
(
    RM_PMU_RPC_STRUCT_CLK_CLK_DOMAIN_PROG_FREQ_QUANTIZE *pParams
)
{
    CLK_DOMAIN         *pDomain =
        (CLK_DOMAIN *)BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, pParams->clkDomainIdx);
    CLK_DOMAIN_PROG    *pDomainProg;
    FLCN_STATUS         status;

    // Sanity check the input clock domain index.
    if ((pDomain == NULL) ||
        (BOARDOBJ_GET_TYPE(pDomain) == LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto pmuRpcClkClkDomainProgFreqQuantize_exit;
    }

    pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
    if (pDomainProg == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto pmuRpcClkClkDomainProgFreqQuantize_exit;
    }

    // Always set default flag to true on external client.
    pParams->input.flags = FLD_SET_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS,
                            _VF_POINT_SET_DEFAULT, _YES, pParams->input.flags);
    status = clkDomainProgFreqQuantize(pDomainProg,
                                       &pParams->input,
                                       &pParams->output,
                                       pParams->bFloor,
                                       LW_TRUE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pmuRpcClkClkDomainProgFreqQuantize_exit;
    }

pmuRpcClkClkDomainProgFreqQuantize_exit:
    return status;
}

/*!
 * @brief   Exelwtes generic CLK_DOMAIN_PROG_CLIENT_FREQ_DELTA_ADJ RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_CLK_CLK_DOMAIN_PROG_CLIENT_FREQ_DELTA_ADJ
 */
FLCN_STATUS
pmuRpcClkClkDomainProgClientFreqDeltaAdj
(
    RM_PMU_RPC_STRUCT_CLK_CLK_DOMAIN_PROG_CLIENT_FREQ_DELTA_ADJ *pParams
)
{
    CLK_DOMAIN         *pDomain =
        (CLK_DOMAIN *)BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, pParams->clkDomainIdx);
    CLK_DOMAIN_PROG    *pDomainProg;
    FLCN_STATUS         status;

    // Sanity check the input clock domain index.
    if ((pDomain == NULL) ||
        (BOARDOBJ_GET_TYPE(pDomain) == LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto pmuRpcClkClkDomainProgClientFreqDeltaAdj_exit;
    }

    pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
    if (pDomainProg == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto pmuRpcClkClkDomainProgClientFreqDeltaAdj_exit;
    }

    status = clkDomainProgClientFreqDeltaAdj(pDomainProg, &pParams->freqMHz);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pmuRpcClkClkDomainProgClientFreqDeltaAdj_exit;
    }

pmuRpcClkClkDomainProgClientFreqDeltaAdj_exit:
    return status;
}

/*!
 * @brief   This command enumerates the frequencies supported on the
 *          given CLK_DOMAIN, as specified by a CLK_DOMAIN index in
 *          to the RM/VBIOS Clocks Table.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_CLK_CLK_DOMAIN_PROG_FREQS_ENUM
 */
FLCN_STATUS
pmuRpcClkClkDomainProgFreqsEnum
(
    RM_PMU_RPC_STRUCT_CLK_CLK_DOMAIN_PROG_FREQS_ENUM *pParams
)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
    {
        CLK_DOMAIN         *pDomain   =
            (CLK_DOMAIN *)BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, pParams->clkDomainIdx);
        CLK_DOMAIN_PROG    *pDomainProg;
        LwU16               freqMHzIt = CLK_DOMAIN_PROG_ITERATE_MAX_FREQ;
        LwU8                i;

        // Sanity check the input clock domain index.
        if ((pDomain == NULL) ||
            (BOARDOBJ_GET_TYPE(pDomain) == LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto pmuRpcClkClkDomainProgFreqsEnum_exit;
        }

        pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
        if (pDomainProg == NULL)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto pmuRpcClkClkDomainProgFreqsEnum_exit;
        }

        // Iterate over set of frequency enumeration and store them.
        while (clkDomainProgFreqEnumIterate(pDomainProg, &freqMHzIt) == FLCN_OK)
        {
            // Sanity check for array bound.
            if (pParams->numFreqs >= LW2080_CTRL_CLK_CLK_DOMAIN_MAX_FREQS)
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto pmuRpcClkClkDomainProgFreqsEnum_exit;
            }

            // Copy-in the frequency.
            pParams->freqsMHz[pParams->numFreqs++] = freqMHzIt;
        }

        // Rearrange the results in output array to store freq in increasing order.
        for (i = 0; i < (pParams->numFreqs / 2); i++)
        {
            LwU16 freqMHzTemp;

            freqMHzTemp = pParams->freqsMHz[(pParams->numFreqs - 1) - i];

            pParams->freqsMHz[(pParams->numFreqs - 1) - i] = pParams->freqsMHz[i];

            pParams->freqsMHz[i] = freqMHzTemp;
        }
    }

pmuRpcClkClkDomainProgFreqsEnum_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
