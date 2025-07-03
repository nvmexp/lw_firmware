/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2_OBJRTTIMER_H
#define SEC2_OBJRTTIMER_H

/*!
 * @file   sec2_rttimer.h
 * @brief  SEC2 RTTimer Library HAL header file.
 */

/* ------------------------ System includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"

/* ------------------------ Application includes --------------------------- */
#include "sec2_rttimer.h"
#include "config/sec2-config.h"
#include "config/g_rttimer_hal.h"

/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */

extern LwrtosSemaphoreHandle RttimerMutex;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

void rttimerPreInit(void)   GCC_ATTRIB_SECTION("imem_init", "rttimerPreInit");

#endif // SEC2_OBJRTTIMER_H
