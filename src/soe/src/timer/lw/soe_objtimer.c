/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soe_objtimer.c
 * @brief  Top-level timer object for timer functionality and HAL infrastructure.
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------- Application Includes --------------------------- */
#include "soe_objhal.h"
#include "soe_objtimer.h"
#include "soe_timer.h"
#include "config/g_timer_hal.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
OBJTIMER Timer;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */


/*!
 * Construct the TIMER object.  This sets up the HAL interface used by the
 * TIMER module.
 */
void
constructTimer(void)
{
    //
    // Setup the HAL interfaces.
    //
    IfaceSetup->timerHalIfacesSetupFn(&Timer.hal);
}

/*!
 * @brief Initialization for the timer engine.
 * 
 * TODO:  Add a Timer Mutex for sharing GPTIMER among different tasks.
 *        This is safe at the moment since only thermal task uses GPTMR.
 *        When Another task wants GPTMR, Mutex must be added for sharing this resource.
 *        Tracked in Bug 200551627.
 */
void
timerPreInit(void)
{
    timerSetup_HAL(&Timer, TIMER_MODE_SETUP,
        TIMER_EVENT_NONE, 0x0, 0x0);
}
