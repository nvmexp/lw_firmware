/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
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
 * @copydoc ClkDomainsProgFreqPropagate
 */
FLCN_STATUS
clkDomainsProgFreqPropagate_35
(
    CLK_DOMAIN_PROG_FREQ_PROP_IN_OUT_TUPLE *pFreqPropTuple,
    LwBoardObjIdx                           clkPropTopIdx
)
{
    CLK_DOMAIN         *pDomain;
    CLK_DOMAIN_PROG    *pDomainProg;
    LwU32               vfRailMinsIntersect = LW_U32_MIN;
    LwBoardObjIdx       d;
    LwU8                voltRailIdx         = 0; // PP-TODO - Do not hardcode.
    FLCN_STATUS         status              = FLCN_OK;

    LW2080_CTRL_CLK_VF_INPUT  vfInput;
    LW2080_CTRL_CLK_VF_OUTPUT vfOutput;

    //
    // CLK_DOMAINS_35 does not support CLK_PROP_TOP objects,
    // so intentionally ignoring this parameter.
    //
    (void)clkPropTopIdx;

    //
    // Loop over all set CLK_DOMAINs and callwlate their Vmins and include
    // them in the Vmins for intersect.
    //
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
        &pFreqPropTuple->pDomainsMaskIn->super)
    {
        pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
        if (pDomainProg == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto clkDomainsProgFreqPropagate_35_exit;
        }

        // Pack VF input and output structs.
        LW2080_CTRL_CLK_VF_INPUT_INIT(&vfInput);
        LW2080_CTRL_CLK_VF_OUTPUT_INIT(&vfOutput);

        // Init
        vfInput.value = pFreqPropTuple->freqMHz[d];

        // Call into CLK_DOMAIN_PROG freqToVolt() interface.
        status = clkDomainProgFreqToVolt(
                    pDomainProg,
                    voltRailIdx,
                    LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,
                    &vfInput,
                    &vfOutput,
                    NULL);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomainsProgFreqPropagate_35_exit;
        }

        //
        // Catch errors when no frequency >= input frequency.  Can't determine the
        // required voltage in this case.
        //
        if (LW2080_CTRL_CLK_VF_VALUE_ILWALID == vfOutput.inputBestMatch)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_OUT_OF_RANGE;
            goto clkDomainsProgFreqPropagate_35_exit;
        }

        // Include the Vmin in the intersect voltage callwlation.
        vfRailMinsIntersect =
            LW_MAX(vfRailMinsIntersect, vfOutput.value);
    }
    BOARDOBJGRP_ITERATOR_END;

    //
    // Loop over all unset strict CLK_DOMAINs and use voltage intersect to
    // determine their TUPLES' target frequencies.
    //
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
        &pFreqPropTuple->pDomainsMaskOut->super)
    {
        pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
        if (pDomainProg == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto clkDomainsProgFreqPropagate_35_exit;
        }

        // Pack VF input and output structs.
        LW2080_CTRL_CLK_VF_INPUT_INIT(&vfInput);
        LW2080_CTRL_CLK_VF_OUTPUT_INIT(&vfOutput);

        // 
        // Init
        // bDefault = LW_FALSE - Voltage intersection to CLK_DOMAINs with sparse
        //                       VF points can lead to choosing frequencies which
        //                       would raise the PSTATE.
        //
        vfInput.value = vfRailMinsIntersect;

        // Call into CLK_DOMAIN_PROG voltToFreq() interface.
        status = clkDomainProgVoltToFreq(
                    pDomainProg,
                    voltRailIdx,
                    LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,
                    &vfInput,
                    &vfOutput,
                    NULL);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomainsProgFreqPropagate_35_exit;
        }

        // Update the output
        if (LW2080_CTRL_CLK_VF_VALUE_ILWALID == vfOutput.inputBestMatch)
        {
            // This is not an error. Client (Arbiter) will set the default values.
            pFreqPropTuple->freqMHz[d] = LW2080_CTRL_CLK_VF_VALUE_ILWALID;
        }
        else
        {
            pFreqPropTuple->freqMHz[d] = vfOutput.value;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

clkDomainsProgFreqPropagate_35_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgIsSecVFLwrvesEnabled
 */
FLCN_STATUS
clkDomainProgIsSecVFLwrvesEnabled_35
(
    CLK_DOMAIN_PROG                    *pDomainProg,
    LwBool                             *pBIsSecVFLwrvesEnabled
)
{
    CLK_DOMAIN_35_PROG *pDomain35Prog   =
        (CLK_DOMAIN_35_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);

    *pBIsSecVFLwrvesEnabled =
        (clkDomain35ProgGetClkDomailwfLwrveCount(pDomain35Prog) > 1) ?
        LW_TRUE :
        LW_FALSE;

    return FLCN_OK;
}

/* ------------------------- Private Functions ----------------------------- */
