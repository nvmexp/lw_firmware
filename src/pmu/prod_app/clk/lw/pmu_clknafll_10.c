/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clknafll_10.c
 * @brief  File for CLK NAFLL routines supported only on version _10.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Function Prototypes --------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct a NAFLL_DEVICE_V10 object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkNafllGrpIfaceModel10ObjSet_V10
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    CLK_NAFLL_DEVICE   *pNafllDev;
    FLCN_STATUS         status;
    LwBool              bFirstConstruct = (*ppBoardObj == NULL);
    RM_PMU_CLK_CLK_NAFLL_DEVICE_BOARDOBJ_SET *pTmpDesc =
        (RM_PMU_CLK_CLK_NAFLL_DEVICE_BOARDOBJ_SET *)pDesc;

    // Construct and initialize parent-class object.
    status = clkNafllGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkNafllGrpIfaceModel10ObjSet_V10_exit;
    }
    pNafllDev = (CLK_NAFLL_DEVICE *)*ppBoardObj;

    //
    // Initialize parameters which are required to be initialized just for the
    // first time construction.
    //
    if (bFirstConstruct)
    {
        // Allocate the struct containing the dynamic status parameters of NAFLL.
        if (pNafllDev->pStatus == NULL)
        {
            pNafllDev->pStatus =
                lwosCallocType(pBObjGrp->ovlIdxDmem, 1,
                               LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL);
            if (pNafllDev->pStatus == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_NO_FREE_MEM;
                goto clkNafllGrpIfaceModel10ObjSet_V10_exit;
            }
        }

        //
        // The DVCO is enabled by default in hardware. Set the corresponding
        // software enable state to true
        //
        pNafllDev->dvco.bEnabled   = LW_TRUE;

        // Flag indicating DVCO min got hit is false by default
        pNafllDev->pStatus->dvco.bMinReached = LW_FALSE;

        status = clkNafllRegMapInit_HAL(&Clk, pNafllDev);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkNafllGrpIfaceModel10ObjSet_V10_exit;
        }

        pNafllDev->pStatus->pldiv = clkNafllPldivGet_HAL(&Clk, pNafllDev);

        //
        // Get the lwrrently set VFGAIN from the LUT override register and
        // cache it in the pNafllDev structure. This value will be used later
        // on to populate the LUTs as well as to set the SW override in FR and
        // FFR mode.
        // Note: The big assumption is that the VBIOS will always set this value.
        //
        status = clkNafllLut10OverrideGet_HAL(&Clk, pNafllDev, NULL, NULL,
                                                &pNafllDev->vfGain);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkNafllGrpIfaceModel10ObjSet_V10_exit;
        }
    }

    // Note: RM should sanity check that these indices point to valid devices
    pNafllDev->pLogicAdcDevice = CLK_ADC_DEVICE_GET(pTmpDesc->adcIdxLogic);
    if (pNafllDev->pLogicAdcDevice == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkNafllGrpIfaceModel10ObjSet_V10_exit;
    }

    // Sram device may or may not be present
    if (pTmpDesc->adcIdxSram != LW2080_CTRL_CLK_ADC_ID_UNDEFINED)
    {
        pNafllDev->pSramAdcDevice  = CLK_ADC_DEVICE_GET(pTmpDesc->adcIdxSram);
        if (pNafllDev->pSramAdcDevice == NULL)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkNafllGrpIfaceModel10ObjSet_V10_exit;
        }
    }

    // Construct a LUT device per NAFLL
    status = clkNafllLutConstruct(pNafllDev, &pNafllDev->lutDevice,
        &pTmpDesc->lutDevice, bFirstConstruct);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkNafllGrpIfaceModel10ObjSet_V10_exit;
    }

    status = clkNafllRegimeInit(pNafllDev, &pTmpDesc->regimeDesc,
        bFirstConstruct);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkNafllGrpIfaceModel10ObjSet_V10_exit;
    }

clkNafllGrpIfaceModel10ObjSet_V10_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
