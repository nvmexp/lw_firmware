/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soe_icls10.c
 * @brief  Contains all Interrupt Control routines specific to SOE LR10x.
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "main.h"
#include "dev_soe_csb.h"
#include "soe_objsoe.h"

#include "config/g_ic_private.h"
#include "config/g_intr_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */

/*!
 * Enable SAW/GIN EXTIO interrupts
 */
void
icEnableGinSaw_LS10(void)
{
    // Unmask GIN interrupt (routed into SOE on _HOST_STALLINTR)
    REG_FLD_WR_DRF_DEF_STALL(CSB, _CSOE, _MISC_EXTIO_IRQMSET, _HOST_STALLINTR, _SET);
}

/*!
 * Route extirq8 interrupt to SAW/GIN engine
 */
void
icRouteGinSaw_LS10(void)
{
    LwU32 extio;
    
    extio =  REG_RD32(CSB, LW_CSOE_MISC_EXTIO_IRQSTAT);
    if (FLD_TEST_DRF(_CSOE, _MISC_EXTIO_IRQSTAT, _HOST_STALLINTR, _TRUE, extio))
    {
        // Service local HOST_NOSTALLINTR interrupt.
        intrService_HAL();
    }
    else 
    {
        // Unexpected interrupt on EXTIRQ8, halt for now.
        // TODO: Provide the reason for the halt to the handler
        SOE_HALT();
    }    
}
