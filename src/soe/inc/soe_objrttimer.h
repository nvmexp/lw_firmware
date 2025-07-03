/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SOE_OBJRTTIMER_H
#define SOE_OBJRTTIMER_H

/*!
 * @file   soe_rttimer.h
 * @brief  SOE RTTimer Library HAL header file.
 */

/* ------------------------ System includes -------------------------------- */
#include "lwuproc.h"
#include "soesw.h"

/* ------------------------ Application includes --------------------------- */
#include "soe_rttimer.h"
#include "config/soe-config.h"
#include "config/g_rttimer_hal.h"

/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Types definitions ------------------------------ */

typedef struct
{
    RTTIMER_HAL_IFACES hal;
} OBJRTTIMER;

/* ------------------------ External definitions --------------------------- */

extern OBJRTTIMER Rttimer;
extern LwrtosSemaphoreHandle RttimerMutex;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

void rttimerPreInit(void)   GCC_ATTRIB_SECTION("imem_init", "rttimerPreInit");

#endif // SOE_OBJRTTIMER_H
