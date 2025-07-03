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
 * @file   pmu_clknafll_20.c
 * @brief  File for CLK NAFLL routines supported only on version _20.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"

#include "clk3/dom/clkfreqdomain_singlenafll.h"
#include "clk3/dom/clkfreqdomain_multinafll.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Function Prototypes --------------------- */
/* ------------------------- Private Functions ------------------------------ */
static BoardObjVirtualTableDynamicCast (s_clkNafllDeviceDynamicCast_V20)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "s_clkNafllDeviceDynamicCast_V20")
    GCC_ATTRIB_USED();

/* ------------------------- Global Variables ------------------------------- */
/*!
 * Virtual table for CLK_NAFLL_DEVICE_V20 object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE ClkNafllDeviceV20VirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_clkNafllDeviceDynamicCast_V20)
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Construct a NAFLL_DEVICE_V20 object.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
clkNafllGrpIfaceModel10ObjSet_V20
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    CLK_NAFLL_DEVICE   *pNafllDev;
    FLCN_STATUS         status;
    LwBool              bFirstConstruct = (*ppBoardObj == NULL);
    RM_PMU_CLK_CLK_NAFLL_DEVICE_BOARDOBJ_SET *pTmpDesc =
        (RM_PMU_CLK_CLK_NAFLL_DEVICE_BOARDOBJ_SET *)pBoardObjDesc;

    // Construct and initialize parent-class object.
    status = clkNafllGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkNafllGrpIfaceModel10ObjSet_V20_exit;
    }
    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pNafllDev, *ppBoardObj, CLK, NAFLL_DEVICE, BASE,
                              &status, clkNafllGrpIfaceModel10ObjSet_V20_exit);

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLKS_ON_PMU))
    // Attach overlays
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clk3x)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList); // NJ??
#endif

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
                lwosCallocType(pBObjGrp->ovlIdxDmem, 1U,
                               LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL);
            if (pNafllDev->pStatus == NULL)
            {
                status = FLCN_ERR_NO_FREE_MEM;
                PMU_BREAKPOINT();
                goto clkNafllGrpIfaceModel10ObjSet_V20_exit;
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
            goto clkNafllGrpIfaceModel10ObjSet_V20_exit;
        }

        pNafllDev->pStatus->pldiv = clkNafllPldivGet_HAL(&Clk, pNafllDev);
    }

    // Note: RM should sanity check that these indices point to valid devices
    pNafllDev->pLogicAdcDevice = CLK_ADC_DEVICE_GET(pTmpDesc->adcIdxLogic);
    if (pNafllDev->pLogicAdcDevice == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkNafllGrpIfaceModel10ObjSet_V20_exit;
    }

    // Sram device may or may not be present
    if (pTmpDesc->adcIdxSram != LW2080_CTRL_CLK_ADC_ID_UNDEFINED)
    {
        pNafllDev->pSramAdcDevice  = CLK_ADC_DEVICE_GET(pTmpDesc->adcIdxSram);
        if (pNafllDev->pSramAdcDevice == NULL)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkNafllGrpIfaceModel10ObjSet_V20_exit;
        }
    }

    // Construct a LUT device per NAFLL
    status = clkNafllLutConstruct(pNafllDev, &pNafllDev->lutDevice,
        &pTmpDesc->lutDevice, bFirstConstruct);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkNafllGrpIfaceModel10ObjSet_V20_exit;
    }

    status = clkNafllRegimeInit(pNafllDev, &pTmpDesc->regimeDesc,
        bFirstConstruct);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkNafllGrpIfaceModel10ObjSet_V20_exit;
    }

clkNafllGrpIfaceModel10ObjSet_V20_exit:

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLKS_ON_PMU))
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
#endif

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   CLK_NAFLL_DEVICE_V20 implementation of @ref BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_clkNafllDeviceDynamicCast_V20
(
    BOARDOBJ *pBoardObj,
    LwU8      requestedType
)
{
    CLK_NAFLL_DEVICE_V20 *pClkNafllDeviceV20 = (CLK_NAFLL_DEVICE_V20 *)pBoardObj;
    void                 *pObject            = NULL;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(CLK, NAFLL_DEVICE, V20))
    {
        PMU_BREAKPOINT();
        goto s_clkNafllDeviceDynamicCast_v20_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(CLK, NAFLL_DEVICE, BASE):
        {
            CLK_NAFLL_DEVICE *pClkNafllDevice = &(pClkNafllDeviceV20->super);
            pObject = (void *)pClkNafllDevice;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(CLK, NAFLL_DEVICE, V20):
        {
            pObject = (void *)pClkNafllDeviceV20;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_clkNafllDeviceDynamicCast_v20_exit:
    return pObject;
}

