/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SOE_OBJTIMER_H
#define SOE_OBJTIMER_H

/*!
 * @file   soe_objtimer.h
 * @brief  SOE Timer Library HAL header file.
 */

/* ------------------------ System includes -------------------------------- */
#include "lwuproc.h"
#include "soesw.h"

/* ------------------------ Application includes --------------------------- */
#include "soe_timer.h"
#include "config/soe-config.h"
#include "config/g_timer_hal.h"

/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Types definitions ------------------------------ */

typedef struct
{
    TIMER_HAL_IFACES hal;
    TIMER_EVENT event;
} OBJTIMER;

/* ------------------------ External definitions --------------------------- */
extern OBJTIMER Timer;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

void timerPreInit(void)   GCC_ATTRIB_SECTION("imem_init", "timerPreInit");
void constructTimer(void) GCC_ATTRIB_SECTION("imem_init", "constructTimer");

#endif // SOE_OBJTIMER_H
