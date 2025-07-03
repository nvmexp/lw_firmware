/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   ic_miscio_GMXXX.c
 * @brief  Contains all Interrupt Control routines for MISCIO specific to GKXXX.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objic.h"
#include "config/g_ic_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * This is main interrupt handler for all MISCIO (GPIO) interrupts.  Both
 * rising- and falling- edge interrupts are detected and handled here.
 */
void
icServiceMiscIO_GMXXX(void)
{
    icServiceMiscIOBank0_HAL(&Ic);
    icServiceMiscIOBank1_HAL(&Ic);

    if (PMUCFG_FEATURE_ENABLED(PMU_IC_IO_BANK_2_SUPPORTED))
    {
        icServiceMiscIOBank2_HAL(&Ic);
    }
}
