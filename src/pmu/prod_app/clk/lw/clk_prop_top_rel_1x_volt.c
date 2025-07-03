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
 * @file clk_prop_top_rel_1x_volt.c
 *
 * @brief Module managing all state related to the CLK_PROP_TOP_REL_1X_VOLT structure.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "dmemovl.h"

/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_PROP_TOP_REL_1X_VOLT class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkPropTopRelGrpIfaceModel10ObjSet_1X_VOLT
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_CLK_CLK_PROP_TOP_REL_1X_VOLT_BOARDOBJ_SET *pRel1xVoltSet =
        (RM_PMU_CLK_CLK_PROP_TOP_REL_1X_VOLT_BOARDOBJ_SET *)pBoardObjSet;
    CLK_PROP_TOP_REL_1X_VOLT *pRel1xVolt;
    FLCN_STATUS               status;

    status = clkPropTopRelGrpIfaceModel10ObjSet_1X(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        // Super class triggers PMU Break point on error.
        goto clkPropTopRelGrpIfaceModel10ObjSet_1X_VOLT_exit;
    }
    pRel1xVolt = (CLK_PROP_TOP_REL_1X_VOLT *)*ppBoardObj;

    // Copy the CLK_PROP_TOP_REL_1X_VOLT-specific data.
    pRel1xVolt->voltRailIdx = pRel1xVoltSet->voltRailIdx;

clkPropTopRelGrpIfaceModel10ObjSet_1X_VOLT_exit:
    return status;
}

/*!
 * @copydoc ClkPropTopRelFreqPropagate
 */
FLCN_STATUS
clkPropTopRelFreqPropagate_1X_VOLT
(
    CLK_PROP_TOP_REL   *pPropTopRel,
    LwBool              bSrcToDstProp,
    LwU16              *pFreqMHz
)
{
    CLK_PROP_TOP_REL_1X_VOLT   *pRel1xVolt = (CLK_PROP_TOP_REL_1X_VOLT *)pPropTopRel;
    CLK_DOMAIN                 *pDomain    = NULL;
    CLK_DOMAIN_PROG            *pDomainProg;
    LW2080_CTRL_CLK_VF_INPUT    freqMHzInput;
    LW2080_CTRL_CLK_VF_OUTPUT   voltuVOutput;
    LW2080_CTRL_CLK_VF_INPUT    voltuVInput;
    LW2080_CTRL_CLK_VF_OUTPUT   freqMHzOutput;
    LwU8                        clkDomainIdxSrc = pRel1xVolt->super.super.clkDomainIdxSrc;
    LwU8                        clkDomainIdxDst = pRel1xVolt->super.super.clkDomainIdxDst;
    FLCN_STATUS                 status          = FLCN_ERR_NOT_SUPPORTED;

    // Switch the Src and Dst if requested propagation from Dst -> Src.
    if (!bSrcToDstProp)
    {
        clkDomainIdxSrc = pRel1xVolt->super.super.clkDomainIdxDst;
        clkDomainIdxDst = pRel1xVolt->super.super.clkDomainIdxSrc;
    }

    // Colwert Scr clock domain
    pDomain = CLK_DOMAIN_GET(clkDomainIdxSrc);
    if (pDomain == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkPropTopRelFreqPropagate_1X_VOLT_exit;
    }

    pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
    if (pDomainProg == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkPropTopRelFreqPropagate_1X_VOLT_exit;
    }

    // Pack VF input and output structs.
    LW2080_CTRL_CLK_VF_INPUT_INIT(&freqMHzInput);
    LW2080_CTRL_CLK_VF_OUTPUT_INIT(&voltuVOutput);

    //
    // The POR could have more than 100% propagation raio relationship.
    // In this case, the propagated frequency may go beyond the VF lwrve
    // max. We need to get the default value out in this case.
    //
    freqMHzInput.flags = FLD_SET_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS,
                            _VF_POINT_SET_DEFAULT, _YES, freqMHzInput.flags);

    // Update the input frequency.
    freqMHzInput.value = (*pFreqMHz);

    // Call into CLK_DOMAIN_PROG freqToVolt() interface.
    status = clkDomainProgFreqToVolt(
                pDomainProg,
                pRel1xVolt->voltRailIdx,
                LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,
                &freqMHzInput,
                &voltuVOutput,
                NULL);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkPropTopRelFreqPropagate_1X_VOLT_exit;
    }

    //
    // Catch errors when no frequency >= input frequency.  Can't determine the
    // required voltage in this case.
    //
    if (LW2080_CTRL_CLK_VF_VALUE_ILWALID == voltuVOutput.inputBestMatch)
    {
        status = FLCN_ERR_OUT_OF_RANGE;
        PMU_BREAKPOINT();
        goto clkPropTopRelFreqPropagate_1X_VOLT_exit;
    }

    pDomain = CLK_DOMAIN_GET(clkDomainIdxDst);
    if (pDomain == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkPropTopRelFreqPropagate_1X_VOLT_exit;
    }

    pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
    if (pDomainProg == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkPropTopRelFreqPropagate_1X_VOLT_exit;
    }

    // Pack VF input and output structs.
    LW2080_CTRL_CLK_VF_INPUT_INIT(&voltuVInput);
    LW2080_CTRL_CLK_VF_OUTPUT_INIT(&freqMHzOutput);

    voltuVInput.value = voltuVOutput.value;

    // Call into CLK_DOMAIN_PROG voltToFreq() interface.
    status = clkDomainProgVoltToFreq(
                pDomainProg,
                pRel1xVolt->voltRailIdx,
                LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,
                &voltuVInput,
                &freqMHzOutput,
                NULL);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkPropTopRelFreqPropagate_1X_VOLT_exit;
    }

    //
    // We use clock propagation from each individual known src clock domain
    // to unknown dst clock domain and return the MAX all propagated frequency
    // of unknown clock domain. So if one of the known src clock domain
    // frequency is very low say GPC is at P8.min = 200 MHz, when we propagate
    // from GPC 200 Mhz -> LWVDD VOLT -> DRAM Frequency, the LWVDD voltage
    // maynot be enough to support the lowest frequency of DRAM clock. This is
    // expected and should not be consider as error.
    //

    //
    // When inputBestMask == _ILWALID, no VF lwrve voltage <= input voltage.
    //
    if (LW2080_CTRL_CLK_VF_VALUE_ILWALID == freqMHzOutput.inputBestMatch)
    {
        // This is not an error. Client (Arbiter) will set the default values.
        (*pFreqMHz) = LW2080_CTRL_CLK_VF_VALUE_ILWALID;
    }
    else
    {
        // Copy out the maximum frequency (kHz) for the CLK_DOMAIN.
        (*pFreqMHz) = freqMHzOutput.value;
    }

clkPropTopRelFreqPropagate_1X_VOLT_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
