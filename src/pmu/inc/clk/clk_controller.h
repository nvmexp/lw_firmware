/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_CONTROLLER_H
#define CLK_CONTROLLER_H

/*!
 * @file clk_controller.h
 * @brief @copydoc clk_controller.c
 */

/* ------------------------ Includes ---------------------------------------- */
/* ------------------------ Macros ------------------------------------------ */
/* ------------------------ Defines ----------------------------------------- */
/* ------------------------ Datatypes --------------------------------------- */
/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
FLCN_STATUS    clkClkControllersLoad(RM_PMU_CLK_LOAD *pClkLoad)
    GCC_ATTRIB_SECTION("imem_libClkController", "clkClkControllersLoad");
FLCN_STATUS    perfClkControllersIlwalidate(LwBool bOverrideVoltOffset)
    GCC_ATTRIB_SECTION("imem_libClkController", "perfClkControllersIlwalidate");
/* ------------------------ Include Derived Types --------------------------- */
#include "clk/clk_freq_controller.h"
#include "clk/clk_volt_controller.h"

#endif // CLK_CONTROLLER_H
